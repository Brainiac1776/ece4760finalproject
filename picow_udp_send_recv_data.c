/**
 * Copyright (c) 2022 Andrew McDonnell
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *
 */
/*
 * UDP send/receive Adapted from:
 ** Copyright (c) 2016 Stephan Linz <linz@li-pro.net>, Li-Pro.Net
 * All rights reserved.
 *
 * Based on examples provided by
 * Iwan Budi Kusnanto <ibk@labhijau.net> (https://gist.github.com/iwanbk/1399729)
 * Juri Haberland <juri@sapienti-sat.org> (https://lists.gnu.org/archive/html/lwip-users/2007-06/msg00078.html)
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of and a contribution to the lwIP TCP/IP stack.
 *
 * Credits go to Adam Dunkels (and the current maintainers) of this software.
 *
 * Stephan Linz rewrote this file to get a basic echo example.
 *
 * =============================================================
 * UDP send/recv code is from :
 * Pico examples
 * https://github.com/raspberrypi/pico-examples/tree/master/pico_w/wifi/udp_beacon
 * lwip contrib apps
 * https://github.com/lwip-tcpip/lwip/tree/master/contrib/apps
 * UDP send/recv on Windows is from:
 * Microsoft
 * https://apps.microsoft.com/store/detail/udp-senderreciever/9NBLGGH52BT0?hl=en-us&gl=us
 * a bare-bones packet whacker
 * =============================================================
 * Threads:
 * -- udp send
 * -- udp recv
 * -- blink cyw43 LED
 * -- serial for debug, set the mode to 'send, echo' and set blink time in 'send' mode
 * figure out addresses
 * -- pico discovery:
 *      in send mode: broadcast sender's IP address. format IP xxx.xxx.xxx.xxx
 *      pico in echo mode: sees braodcast and sends it's IP address back to sender's IP addr
 */

#include <string.h>
#include <stdlib.h>
#include <pico/multicore.h>
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "stdio.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/dns.h"
#include "lwip/netif.h"

// ======================================
// udp constants
#define UDP_PORT 4444
#define UDP_MSG_LEN_MAX 1024
#define UDP_TARGET_DESK "192.168.1.9" // desktop
#define UDP_TARGET_BROADCAST "255.255.255.255"

#define RED_BUTTON 15    // GPIO PIN
#define BLUE_BUTTON_16   // GPIO PIN
#define YELLOW_BUTTON 17 // GPIO PIN
#define GREEN_BUTTON 18  // GPIO PIN

void init_buttons()
{
  gpio_init(RED_BUTTON_PIN);
  gpio_set_dir(RED_BUTTON_PIN, GPIO_IN);
  gpio_pull_up(RED_BUTTON_PIN);

  gpio_init(BLUE_BUTTON_PIN);
  gpio_set_dir(BLUE_BUTTON_PIN, GPIO_IN);
  gpio_pull_up(BLUE_BUTTON_PIN);

  gpio_init(YELLOW_BUTTON_PIN);
  gpio_set_dir(YELLOW_BUTTON_PIN, GPIO_IN);
  gpio_pull_up(YELLOW_BUTTON_PIN);

  gpio_init(GREEN_BUTTON_PIN);
  gpio_set_dir(GREEN_BUTTON_PIN, GPIO_IN);
  gpio_pull_up(GREEN_BUTTON_PIN);
}

// Check if a button is pressed
int is_button_pressed(int pin)
{
  return gpio_get(pin) == 0;
}

// #define UDP_INTERVAL_MS 10 // not used
//  should resolve to a actual addr after pairing
char udp_target_pico[20] = "255.255.255.255";

// choose appropriate packet length
enum packet_lengths
{
  command,
  ack,
  data
} packet_length = command;

// =======================================
// necessary to connect to wireless
// !!! Do NOT post this info !!!
#define WIFI_SSID "miko"
#define WIFI_PASSWORD "123456789"

// =======================================
// protothreads and thread communication
#include "pt_cornell_rp2040_v1_1_2.h"
char recv_data[UDP_MSG_LEN_MAX];
char send_data[UDP_MSG_LEN_MAX];

// payload to led blink
// or send to remote system///////////////////
int blink_time, remote_blink_time;
// interthread communicaition
// signal threads for sned/recv data
struct pt_sem new_udp_recv_s, new_udp_send_s;
// mode: send/echo
// send mode is in chage here, defined by seril input
// both units default to echo
#define echo 0
#define send 1
int mode = echo;
// did the addresses get set up?
int paired = false;
// data to send over WIFI
#define max_data_size 16
int data_size = 16;

struct PSData
{
  int player;
  int sequence[16];
  int lives;
};

struct PSData data_obj;
struct PSData received_data_obj;

// the following MUST be less than or equal to:
// UDP_MSG_LEN_MAX
// but for efficiency, not much smaller
#define send_data_size data_size * sizeof(short)
// record time for packet speed determination
uint64_t time1;

//==================================================
// UDP async receive callback setup
// NOTE that udpecho_raw_recv is triggered by a signal
// directly from the LWIP package -- not from your code
// this callback juswt copies out the packet string
// and sets a "new data" semaphore
// This runs in an ISR -- KEEP IT SHORT!!!

#if LWIP_UDP

static struct udp_pcb *udpecho_raw_pcb;
struct pbuf *p;

static void
udpecho_raw_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                 const ip_addr_t *addr, u16_t port)
{
  LWIP_UNUSED_ARG(arg);

  if (p != NULL)
  {
    // printf("p payload in call back: = %s\n", p->payload);
    memcpy(recv_data, p->payload, UDP_MSG_LEN_MAX);
    // can signal from an ISR -- BUT NEVER wait in an ISR
    PT_SEM_SIGNAL(pt, &new_udp_recv_s);

    /* free the pbuf */
    pbuf_free(p);
  }
  else
    printf("NULL pt in callback");
}

// ===================================
// Define the recv callback
void udpecho_raw_init(void)
{
  udpecho_raw_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  p = pbuf_alloc(PBUF_TRANSPORT, UDP_MSG_LEN_MAX + 1, PBUF_RAM);

  if (udpecho_raw_pcb != NULL)
  {
    err_t err;
    // netif_ip4_addr returns the picow ip address
    err = udp_bind(udpecho_raw_pcb, netif_ip4_addr(netif_list), UDP_PORT); // DHCP addr

    if (err == ERR_OK)
    {
      udp_recv(udpecho_raw_pcb, udpecho_raw_recv, NULL);
      // printf("Set up recv callback\n");
    }
    else
    {
      printf("bind error");
    }
  }
  else
  {
    printf("udpecho_raw_pcb error");
  }
}

#endif /* LWIP_UDP */
// end recv setup

// =======================================
// UDP send thead
// sends data when signalled
// =======================================

void game_logic()
{
  game_init();

  while (lives > 0)
  {
    sequenceLength = 0;
    memset(sequence, 0, sizeof(sequence));

    while (sequenceLength < MAX_SEQUENCE_LENGTH)
    {
      int button = check_buttons();
      if (button != 0)
      {
        sequence[sequenceLength++] = button;
      }
    }

    send_sequence();
    receive_sequence();

    if (!compare_sequences(sequence, (int *)recv_data, sequenceLength))
    {
      lives--;
    }
  }
  printf("Game Over. Lives: %d\n", lives);
}

bool compare_sequences(const int *seq1, const int *seq2, int length1, int length2)
{

  if (length1 != length2)
  {
    return false;
  }

  for (int i = 0; i < length1; i++)
  {
    if (seq1[i] != seq2[i])
    {
      return false;
    }
  }

  return true;
}

static PT_THREAD(protothread_signal_send(struct pt *pt))
{
  PT_BEGIN(pt);

  static struct udp_pcb *pcb;
  pcb = udp_new();
  pcb->remote_port = UDP_PORT;
  pcb->local_port = UDP_PORT;

  static int sequence[MAX_SEQUENCE_LENGTH];
  static int sequenceLength = 0;
  static int lives = INITIAL_LIVES;
  static bool sequenceComplete;

  while (1)
  {
    sequenceLength = 0;
    memset(sequence, 0, sizeof(sequence));
    sequenceComplete = false;

    while (!sequenceComplete && sequenceLength < MAX_SEQUENCE_LENGTH)
    {
      if (gpio_get(SEND_BUTTON_1) == 0)
        sequence[sequenceLength++] = 1;
      else if (gpio_get(SEND_BUTTON_2) == 0)
        sequence[sequenceLength++] = 2;
      else if (gpio_get(SEND_BUTTON_3) == 0)
        sequence[sequenceLength++] = 3;
      else if (gpio_get(SEND_BUTTON_4) == 0)
        sequence[sequenceLength++] = 4;

      if (gpio_get(END_SEQUENCE_BUTTON) == 0)
        sequenceComplete = true;
      PT_YIELD(pt);
    }

    memset(send_data, 0, UDP_MSG_LEN_MAX);
    memcpy(send_data, sequence, sequenceLength * sizeof(int));
    packet_length = data;

    PT_SEM_SIGNAL(pt, &new_udp_send_s);
    PT_YIELD(pt);

    if (compare_sequences(sequence, (int *)recv_data, sequenceLength))
    {
      lives--;
      if (lives <= 0)
      {
        // game over
        PT_EXIT(pt);
      }
    }
  }
  PT_END(pt);
}

static PT_THREAD(protothread_signal_recv(struct pt *pt))
{
  PT_BEGIN(pt);
  static char arg1[32], arg2[32], arg3[32], arg4[32];
  static char *token;

  // data structure for interval timer
  // PT_INTERVAL_INIT() ;

  while (1)
  {

    memset(received_data_obj, 0, UDP_MSG_LEN_MAX);
    memcpy(received_data_obj, received_data_obj.sequence, send_data_size);

    sprintf(received_data_obj, "ack");
    packet_length = ack;

    PT_SEM_SIGNAL(pt, &new_udp_send_s);
    PT_YIELD(pt);
    printf("got -- ");

    for (int i = 0; i < data_size; i++)
    {
      printf("%d  ", received_data_obj.sequence[i]);
    }

    bool sameSequence = checkSequence(data_obj.sequence, received_data_obj.sequence);
    if (sameSequence)
    {
      data_obj.lives--;
      printf("same sequence");
    }
    else
    {
      printf("different sequence");
    }

    printf("\n\r");
    PT_YIELD(pt);
  }
  PT_END(pt);
}

static PT_THREAD(protothread_signal_recv(struct pt *pt))
{
  PT_BEGIN(pt);

  static struct udp_pcb *recv_pcb;
  static struct pbuf *p;
  static ip_addr_t from_ip;
  static u16_t from_port;

  recv_pcb = udp_new();
  udp_bind(recv_pcb, IP_ADDR_ANY, UDP_PORT);

  while (1)
  {
    p = udp_recv(recv_pcb, &from_ip, &from_port, 0);
    if (p != NULL)
    {
      memset(received_data_obj.sequence, 0, sizeof(received_data_obj.sequence));
      pbuf_copy_partial(p, received_data_obj.sequence, p->tot_len, 0); // Copy data to received_data_obj.sequence
      pbuf_free(p);                                                    // Free the packet buffer

      printf("Received sequence: ");
      for (int i = 0; i < data_size; i++)
      {
        printf("%d ", received_data_obj.sequence[i]);
      }
      printf("\n");

      if (!compare_sequences(data_obj.sequence, received_data_obj.sequence, MAX_SEQUENCE_LENGTH, data_size))
      {
        printf("Incorrect sequence\n");
        data_obj.lives--;
        if (data_obj.lives <= 0)
        {
          printf("Game Over\n");
        }
      }
      else
      {
        printf("Correct sequence\n");
      }
    }
    else
    {
      printf("No data received or error in reception\n");
    }

    sprintf(received_data_obj, "ack");
    packet_length = ack;
    PT_SEM_SIGNAL(pt, &new_udp_send_s);

    PT_YIELD(pt);
  }
  PT_END(pt);
}

// ==================================================
// toggle cyw43 LED
// this is really just a test of multitasking
// compatability with LWIP
// but also reads out pair status
// ==================================================
static PT_THREAD(protothread_toggle_cyw43(struct pt *pt))
{
  PT_BEGIN(pt);
  static bool LED_state = false;
  //
  // data structure for interval timer
  PT_INTERVAL_INIT();
  // set some default blink time
  blink_time = 100;
  // echo the default time to udp connection
  // PT_SEM_SIGNAL(pt, &new_udp_send_s) ;

  while (1)
  {
    // force a context switch of there is data to send
    if (&new_udp_send_s.count)
      PT_YIELD(pt);
    //
    LED_state = !LED_state;
    // the onboard LED is attached to the wifi module
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, LED_state);
    // blink time is modifed by the udp recv thread
    PT_YIELD_INTERVAL(blink_time * 1000);
    //
    // NEVER exit while
  } // END WHILE(1)
  PT_END(pt);
} // blink thread

// =================================================
// command thread
// =================================================
static PT_THREAD(protothread_serial(struct pt *pt))
{
  PT_BEGIN(pt);
  static char cmd[16], arg1[16], arg2[16], arg3[16], arg4[16], arg5[16], arg6[16];
  static char *token;
  //
  printf("Type 'help' for commands\n\r");

  while (1)
  {
    // the yield time is not strictly necessary for protothreads
    // but gives a little slack for the async processes
    // so that the output is in the correct order (most of the time)
    PT_YIELD_usec(100000);

    // print prompt
    sprintf(pt_serial_out_buffer, "cmd> ");
    // spawn a thread to do the non-blocking write
    serial_write;

    // spawn a thread to do the non-blocking serial read
    serial_read;
    // tokenize
    token = strtok(pt_serial_in_buffer, "  ");
    strcpy(cmd, token);
    token = strtok(NULL, "  ");
    strcpy(arg1, token);
    token = strtok(NULL, "  ");
    strcpy(arg2, token);
    token = strtok(NULL, "  ");
    strcpy(arg3, token);
    token = strtok(NULL, "  ");
    strcpy(arg4, token);
    token = strtok(NULL, "  ");
    strcpy(arg5, token);
    token = strtok(NULL, "  ");
    strcpy(arg6, token);

    // parse by command
    if (strcmp(cmd, "help") == 0)
    {
      // commands
      printf("set mode [send, recv]\n\r");
      printf("send \n\r");
      printf("pair \n\r");
      printf("ack \n\r");
      // printf("data array_size \n\r");
      //
      //  need start data and end data commands
    }

    // set the unit mode
    else if (strcmp(cmd, "set") == 0)
    {
      if (strcmp(arg1, "recv") == 0)
      {
        mode = echo;
        // zeros the array to make sure the data is
        // actually sent!
        memset(data_obj.sequence, 0, sizeof(data_obj.sequence));
        printf("%d -- zeroed data array \n\r", data_obj.sequence[15]);
      }
      else if (strcmp(arg1, "send") == 0)
        mode = send;
      else
        printf("bad mode");
      // printf("%d\n", mode);
    }

    // else if(strcmp(cmd,"data")==0){
    //  sscanf(arg1, "%d", data_array+1);
    //}

    // identify other pico on the same subnet
    else if (strcmp(cmd, "pair") == 0)
    {
      if (mode == send)
      {
        // broadcast sender's IP addr
        memset(send_data, 0, UDP_MSG_LEN_MAX);
        sprintf(send_data, "IP %s", ip4addr_ntoa(netif_ip4_addr(netif_list)));
        packet_length = command;
        PT_SEM_SIGNAL(pt, &new_udp_send_s);
        // diagnostics:
        printf("send IP %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
        // boradcast until paired
        printf("sendto IP %s\n", udp_target_pico);
        // probably shoulld be some error checking here
        paired = true;
      }
      else
        printf("No pairing in recv mode -- set send\n");
    }

    // send ack packet
    else if (strcmp(cmd, "ack") == 0)
    {
      if (mode == send)
      {
        memset(send_data, 0, UDP_MSG_LEN_MAX);
        sprintf(send_data, "ack");
        packet_length = ack;
        time1 = PT_GET_TIME_usec();
        PT_SEM_SIGNAL(pt, &new_udp_send_s);
        // yield so that send thread gets faster access
        PT_YIELD(pt);
      }
      else
        printf("No ack in recv mode -- set send\n");
    }

    // send an array to the other pico
    else if (strcmp(cmd, "send") == 0)
    {
      if (mode == send)
      {
        // send the big data array
        memset(send_data, 0, UDP_MSG_LEN_MAX);
        memcpy(send_data, data_obj.sequence, send_data_size);
        packet_length = data;
        // test pairing
        printf("sendto IP %s paired=%d\n", udp_target_pico, paired);
        // trigger send threead
        time1 = PT_GET_TIME_usec();
        PT_SEM_SIGNAL(pt, &new_udp_send_s);
        PT_YIELD(pt);
      }
      else
        printf("No send in recv mode -- set send\n");
    }

    // no valid command
    else
      printf("Huh? Type help. \n\r");

    // NEVER exit while
  } // END WHILE(1)

  PT_END(pt);
}

// ====================================================
int main()
{
  // =======================
  // init the serial
  stdio_init_all();

  init_buttons();

  // =======================
  // init the wifi network
  if (cyw43_arch_init())
  {
    printf("failed to initialise\n");
    return 1;
  }

  // hook up to local WIFI
  cyw43_arch_enable_sta_mode();

  // power managment
  // cyw43_wifi_pm(&cyw43_state, CYW43_DEFAULT_PM & ~0xf);

  printf("Connecting to Wi-Fi...\n");
  if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
  {
    printf("failed to connect.\n");
    return 1;
  }
  else
  {
    // optional print addr
    printf("Connected: picoW IP addr: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
  }

  //============================
  // set up UDP recenve ISR handler
  udpecho_raw_init();

  data_obj.player = 1;

  data_obj.sequence[0] = 3;
  data_obj.sequence[1] = 4;
  data_obj.lives = 3;

  printf("initialized player 1");

  PT_SEM_INIT(&new_udp_send_s, 0);
  PT_SEM_INIT(&new_udp_recv_s, 0);

  // printf("Starting threads\n") ;
  //  note that the ORDER of adding the threads is
  //  important here for perfromance with the async
  //  WIFI interface
  // pt_add_thread(protothread_udp_recv);
  // pt_add_thread(protothread_udp_send);
  pt_add_thread(protothread_signal_send);
  pt_add_thread(protothread_signal_recv);
  pt_add_thread(protothread_toggle_cyw43);
  pt_add_thread(protothread_serial);
  //
  // === initalize the scheduler ===============
  pt_schedule_start;

  cyw43_arch_deinit();
  return 0;
}
