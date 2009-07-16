/*
Copyright � 2003 University of Chicago.  All rights reserved.
The Web100 Network Diagnostic Tool (NDT) is distributed subject to
the following license conditions:
SOFTWARE LICENSE AGREEMENT
Software: Web100 Network Diagnostic Tool (NDT)

1. The "Software", below, refers to the Web100 Network Diagnostic Tool (NDT)
(in either source code, or binary form and accompanying documentation). Each
licensee is addressed as "you" or "Licensee."

2. The copyright holder shown above hereby grants Licensee a royalty-free
nonexclusive license, subject to the limitations stated herein and U.S. Government
license rights.

3. You may modify and make a copy or copies of the Software for use within your
organization, if you meet the following conditions: 
    a. Copies in source code must include the copyright notice and this Software
    License Agreement.
    b. Copies in binary form must include the copyright notice and this Software
    License Agreement in the documentation and/or other materials provided with the copy.

4. You may make a copy, or modify a copy or copies of the Software or any
portion of it, thus forming a work based on the Software, and distribute copies
outside your organization, if you meet all of the following conditions: 
    a. Copies in source code must include the copyright notice and this
    Software License Agreement;
    b. Copies in binary form must include the copyright notice and this
    Software License Agreement in the documentation and/or other materials
    provided with the copy;
    c. Modified copies and works based on the Software must carry prominent
    notices stating that you changed specified portions of the Software.

5. Portions of the Software resulted from work developed under a U.S. Government
contract and are subject to the following license: the Government is granted
for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable
worldwide license in this computer software to reproduce, prepare derivative
works, and perform publicly and display publicly.

6. WARRANTY DISCLAIMER. THE SOFTWARE IS SUPPLIED "AS IS" WITHOUT WARRANTY
OF ANY KIND. THE COPYRIGHT HOLDER, THE UNITED STATES, THE UNITED STATES
DEPARTMENT OF ENERGY, AND THEIR EMPLOYEES: (1) DISCLAIM ANY WARRANTIES,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY IMPLIED WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE OR NON-INFRINGEMENT,
(2) DO NOT ASSUME ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
COMPLETENESS, OR USEFULNESS OF THE SOFTWARE, (3) DO NOT REPRESENT THAT USE
OF THE SOFTWARE WOULD NOT INFRINGE PRIVATELY OWNED RIGHTS, (4) DO NOT WARRANT
THAT THE SOFTWARE WILL FUNCTION UNINTERRUPTED, THAT IT IS ERROR-FREE OR THAT
ANY ERRORS WILL BE CORRECTED.

7. LIMITATION OF LIABILITY. IN NO EVENT WILL THE COPYRIGHT HOLDER, THE
UNITED STATES, THE UNITED STATES DEPARTMENT OF ENERGY, OR THEIR EMPLOYEES:
BE LIABLE FOR ANY INDIRECT, INCIDENTAL, CONSEQUENTIAL, SPECIAL OR PUNITIVE
DAMAGES OF ANY KIND OR NATURE, INCLUDING BUT NOT LIMITED TO LOSS OF PROFITS
OR LOSS OF DATA, FOR ANY REASON WHATSOEVER, WHETHER SUCH LIABILITY IS ASSERTED
ON THE BASIS OF CONTRACT, TORT (INCLUDING NEGLIGENCE OR STRICT LIABILITY), OR
OTHERWISE, EVEN IF ANY OF SAID PARTIES HAS BEEN WARNED OF THE POSSIBILITY OF
SUCH LOSS OR DAMAGES.
The Software was developed at least in part by the University of Chicago,
as Operator of Argonne National Laboratory (http://miranda.ctd.anl.gov:7123/). 
 */

#include "../config.h"

#include <time.h>
#include <ctype.h>
#include <math.h>
#define SYSLOG_NAMES
#include  <syslog.h>
#include <pthread.h>
#include <sys/times.h>

#include "web100srv.h"
#include "network.h"
#include "usage.h"
#include "utils.h"
#include "mrange.h"
#include "logging.h"
#include "testoptions.h"
#include "protocol.h"
#include "web100-admin.h"
#include "test_sfw.h"
#include "ndt_odbc.h"

static char lgfn[256];
static char wvfn[256];
static char apfn[256];
static char slfa[256];
static char logd[256];
static char portbuf[10];
static char devicebuf[100];
static char dbDSNbuf[256];
static char dbUIDbuf[256];
static char dbPWDbuf[256];

/* list of global variables used throughout this program. */
int window = 64000;
int randomize=0;
int count_vars=0;
int dumptrace=0;
int usesyslog=0;
int multiple=0;
int max_clients=50;
int set_buff=0;
int admin_view=0;
int queue=1;
int view_flag=0;
int record_reverse=0;
int testing, waiting;
int refresh = 30;
int old_mismatch=0;  /* use the old duplex mismatch detection heuristic */
int sig1, sig2, sig17;

Options options;
CwndPeaks peaks;
int cputime = 0;
char cputimelog[256];
pthread_t workerThreadId, zombieThreadId;
int workerLoop=1, zombie_check=0;

int useDB = 0;
char* dbDSN = NULL;
char* dbUID = NULL;
char* dbPWD = NULL;

char *VarFileName=NULL;
char *AdminFileName=NULL;
char *SysLogFacility=NULL;
int syslogfacility = LOG_FACILITY;
char *ProcessName={"web100srv"};
char *ConfigFileName=NULL;
char buff[BUFFSIZE+1];
char *rmt_host;
char spds[4][256], buff2[32];
char *device=NULL;
char* port = PORT;
TestOptions testopt;

pcap_t *pd;
pcap_dumper_t *pdump;
float run_ave[4];

int conn_options = 0;
struct ndtchild *head_ptr;
int ndtpid;
int testPort;
char testName[256];

static struct option long_options[] = {
  {"adminview", 0, 0, 'a'},
  {"debug", 0, 0, 'd'},
  {"help", 0, 0, 'h'},
  {"multiple", 0, 0, 'm'},
  {"max_clients", 1, 0, 'x'},
  {"mrange", 1, 0, 301},
  {"old", 0, 0, 'o'},
  {"disable-queue", 0, 0, 'q'},
  {"record", 0, 0, 'r'},
  {"syslog", 0, 0, 's'},
  {"tcpdump", 0, 0, 't'},
  {"version", 0, 0, 'v'},
  {"config", 1, 0, 'c'},
#ifdef EXPERIMENTAL_ENABLED
  {"avoidsndblockup", 0, 0, 306},
  {"snaplog", 0, 0, 307},
  {"snapdelay", 1, 0, 305},
  {"cwnddecrease", 0, 0, 308},
  {"cputime", 0, 0, 309},
  {"limit", 1, 0, 'y'},
#endif
  {"buffer", 1, 0, 'b'},
  {"file", 1, 0, 'f'},
  {"interface", 1, 0, 'i'},
  {"log", 1, 0, 'l'},
  {"port", 1, 0, 'p'},
  {"midport", 1, 0, 302},
  {"c2sport", 1, 0, 303},
  {"s2cport", 1, 0, 304},
  {"refresh", 1, 0, 'T'},
  {"adminfile", 1, 0, 'A'},
  {"log_dir", 1, 0, 'L'},
  {"logfacility", 1, 0, 'S'},
#if defined(HAVE_ODBC) && defined(DATABASE_ENABLED) && defined(HAVE_SQL_H)
  {"enableDBlogging", 0, 0, 310},
  {"dbDSN", 1, 0, 311},
  {"dbUID", 1, 0, 312},
  {"dbPWD", 1, 0, 313},
#endif
#ifdef AF_INET6
  {"ipv4", 0, 0, '4'},
  {"ipv6", 0, 0, '6'},
#endif
  {0, 0, 0, 0}
};

/* Process a SIGCHLD signal */
void
child_sig(void)
{
  int pid, status;
  struct ndtchild *tmp1, *tmp2;

  /*
   * avoid zombies, since we run forever
   * Use the wait3() system call with the WNOHANG option.
   */
  tmp1 = head_ptr;
  log_println(2, "Received SIGCHLD signal for active web100srv process [%d]", getpid());

  while ((pid = wait3(&status, WNOHANG, (struct rusage *) 0)) > 0) {
    log_println(3, "wait3() returned %d for PID=%d", status, pid);
    if (WIFEXITED(status) != 0) {
      log_println(3, "wexitstatus = '%d'", WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status) == 1) {
      log_println(3, "wtermsig = %d", WTERMSIG(status));
    }
    if (WIFSTOPPED(status) == 1) {
      log_println(3, "wstopsig = %d", WSTOPSIG(status));
    }
    if (status != 0) 
      return;
    if (multiple == 1)
      return;

    log_println(4, "Attempting to clean up child %d, head pid = %d", pid, head_ptr->pid);
    if (head_ptr->pid == pid) {
      log_println(5, "Child process %d causing head pointer modification", pid);
      tmp1 = head_ptr;
      head_ptr = head_ptr->next;
      free(tmp1);
      testing = 0;
      waiting--;
      log_println(4, "Removing Child from head, decrementing waiting now = %d", waiting);
      return;
    }
    else {
      while (tmp1->next != NULL) {
        if (tmp1->next->pid == pid) {
          log_println(4, "Child process %d causing task list modification", pid);
          tmp2 = tmp1->next;
          tmp1->next = tmp2->next;
          free(tmp2);
          testing = 0;
          waiting--;
          log_println(4, "Removing Child from list, decrementing waiting now = %d", waiting);
          return;
        }
        tmp1 = tmp1->next;
        log_println(6, "Looping through service queue ptr = 0x%x", (u_int64_t) tmp1);
      }
    }
    log_println(3, "SIGCHLD routine finished!");
    return;
  }
}

/* Catch termination signal(s) and print message in log file */
void
cleanup(int signo)
{
  FILE *fp;

  log_println(1, "Signal %d received by process %d", signo, getpid());
  if (get_debuglvl() > 0) {
    fp = fopen(get_logfile(),"a");
    if (fp != NULL) {
      fprintf(fp, "Signal %d received by process %d\n", signo, getpid());
      fclose(fp);
    }
  }
  switch (signo) {
    default:
      fp = fopen(get_logfile(),"a");
      if (fp != NULL) {
        fprintf(fp, "Unexpected signal (%d) received, process (%d) may terminate\n",
            signo, getpid());
        fclose(fp);
      }
      break;
    case SIGSEGV:
      log_println(6, "DEBUG, caught SIGSEGV signal and terminated process (%d)", getpid());
    case SIGINT:
    case SIGTERM:
      exit(0);
    case SIGUSR1:
      log_println(6, "DEBUG, caught SIGUSR1, setting sig1 flag to force exit");
      sig1 = 1;
      check_signal_flags();
      break;

    case SIGUSR2:
      log_println(6, "DEBUG, caught SIGUSR2, setting sig2 flag to force exit");
      sig2 = 1;
      check_signal_flags();
      break;

    case SIGALRM:
      switch (getCurrentTest()) {
        case TEST_MID:
          log_println(6, "Received SIGALRM signal [Middlebox test]");
          break;
        case TEST_C2S:
          log_println(6, "Received SIGALRM signal [C2S throughput test]");
          break;
        case TEST_S2C:
          log_println(6, "Received SIGALRM signal [S2C throughput test]");
          break;
        case TEST_SFW:
          log_println(6, "Received SIGALRM signal [Simple firewall test]");
          break;
      }
      fp = fopen(get_logfile(),"a");
      if (fp  != NULL) {
        fprintf(fp,"Received SIGALRM signal: terminating active web100srv process [%d]",
            getpid());
        switch (getCurrentTest()) {
          case TEST_MID:
            fprintf(fp, " [Middlebox test]\n");
            break;
          case TEST_C2S:
            fprintf(fp, " [C2S throughput test]\n");
            break;
          case TEST_S2C:
            fprintf(fp, " [S2C throughput test]\n");
            break;
          case TEST_SFW:
            fprintf(fp, " [Simple firewall test]\n");
            break;
          default:
            fprintf(fp, "\n");
        }
        fclose(fp);
      }
      exit(0);
    case SIGPIPE:
      fp = fopen(get_logfile(),"a");
      if (fp != NULL) {
        fprintf(fp,"Received SIGPIPE signal: terminating active web100srv process [%d]\n",
            getpid());
        fclose(fp);
      }
      break;

    case SIGHUP:
      /* Initialize Web100 structures */
      count_vars = web100_init(VarFileName);

      /* The administrator view automatically generates a usage page for the
       * NDT server.  This page is then accessable to the general public.
       * At this point read the existing log file and generate the necessary
       * data.  This data is then updated at the end of each test.
       * RAC 3/11/04
       */
      if (admin_view == 1)
        view_init(refresh);
      break;

    case SIGCHLD:
      /* moved actions to child_sig() routine on 3/10/05  RAC
       * Now all we do here is set a flag and return the flag
       * is checked near the top of the main wait loop, so it
       * will only be accessed once and only the testing proces
       * will attempt to do something with it.
       */
      sig17 = 1;
      break;
  }
}

/* LoadConfig routine copied from Internet2. */
static void LoadConfig(char* name, char **lbuf, size_t *lbuf_max)
{
  FILE *conf;
  char keybuf[256], valbuf[256];
  char *key=keybuf, *val=valbuf;
  int rc=0;


  if (!(conf = fopen(ConfigFileName, "r")))
    return;

  log_println(1, " Reading config file %s to obtain options", ConfigFileName);

  while ((rc = I2ReadConfVar(conf, rc, key, val, 256, lbuf, lbuf_max)) > 0) {
    if (strncasecmp(key, "administrator_view", 5) == 0) {
      admin_view = 1;
      continue;
    }

    else if (strncasecmp(key, "multiple_clients", 5) == 0) {
      multiple = 1;
      continue;
    }

    else if (strncasecmp(key, "max_clients", 5) == 0) {
      max_clients = atoi(val);
      continue;
    }

    else if (strncasecmp(key, "record_reverse", 6) == 0) {
      record_reverse = 1;
      continue;
    }

    else if (strncasecmp(key, "write_trace", 5) == 0) {
      dumptrace = 1;
      continue;
    }

    else if (strncasecmp(key, "TCP_Buffer_size", 3) == 0) {
      if (check_int(val, &window)) {
        char tmpText[200];
        snprintf(tmpText, 200, "Invalid window size: %s", val);
        short_usage(name, tmpText);
      }
      set_buff = 1;
      continue;
    }

    else if (strncasecmp(key, "debug", 5) == 0) {
      set_debuglvl(atoi(val));
      continue;
    }

    else if (strncasecmp(key, "variable_file", 6) == 0) {
      sprintf(wvfn, "%s", val);
      VarFileName = wvfn;
      continue;
    }

    else if (strncasecmp(key, "log_file", 3) == 0) {
      sprintf(lgfn, "%s", val);
      set_logfile(lgfn);
      continue;
    }
    
    else if (strncasecmp(key, "admin_file", 10) == 0) {
      sprintf(apfn, "%s", val);
      AdminFileName = apfn;
      continue;
    }
    
    else if (strncasecmp(key, "logfacility", 11) == 0) {
      sprintf(slfa, "%s", val);
      SysLogFacility = slfa;
      continue;
    }

    else if (strncasecmp(key, "device", 5) == 0) {
      snprintf(devicebuf, 100, "%s", val);
      device = devicebuf;
      continue;
    }

    else if (strncasecmp(key, "port", 4) == 0) {
      snprintf(portbuf, 10, "%s", val);
      port = portbuf;
      continue;
    }

    else if (strncasecmp(key, "syslog", 6) == 0) {
      usesyslog = 1;
      continue;
    }

    else if (strncasecmp(key, "disable_FIFO", 5) == 0) {
      queue = 0;
      continue;
    }
    
    else if (strncasecmp(key, "old_mismatch", 3) == 0) {
      old_mismatch = 1;
      continue;
    }

    else if (strncasecmp(key, "cputime", 3) == 0) {
      cputime = 1;
      continue;
    }

    else if (strncasecmp(key, "enableDBlogging", 8) == 0) {
      useDB = 1;
      continue;
    }

    else if (strncasecmp(key, "dbDSN", 5) == 0) {
      snprintf(dbDSNbuf, 255, "%s", val);
      dbDSN = dbDSNbuf;
      continue;
    }

    else if (strncasecmp(key, "dbUID", 5) == 0) {
      snprintf(dbUIDbuf, 255, "%s", val);
      dbUID = dbUIDbuf;
      continue;
    }

    else if (strncasecmp(key, "dbPWD", 5) == 0) {
      snprintf(dbPWDbuf, 255, "%s", val);
      dbPWD = dbPWDbuf;
      continue;
    }

    else if (strncasecmp(key, "cwnddecrease", 5) == 0) {
      options.cwndDecrease = 1;
      options.snaplog = 1;
      continue;
    }

    else if (strncasecmp(key, "snaplog", 5) == 0) {
      options.snaplog = 1;
      continue;
    }

    else if (strncasecmp(key, "avoidsndblockup", 5) == 0) {
      options.avoidSndBlockUp = 1;
      continue;
    }

    else if (strncasecmp(key, "snapdelay", 5) == 0) {
      options.snapDelay = atoi(val);
      continue;
    }
    
    else if (strncasecmp(key, "limit", 5) == 0) {
      options.limit = atoi(val);
      continue;
    }
    
    else if (strncasecmp(key, "refresh", 5) == 0) {
      refresh = atoi(val);
      continue;
    }
    
    else if (strncasecmp(key, "mrange", 6) == 0) {
      if (mrange_parse(val)) {
        char tmpText[300];
        snprintf(tmpText, 300, "Invalid range: %s", val);
        short_usage(name, tmpText);
      }
      continue;
    }

    else if (strncasecmp(key, "midport", 7) == 0) {
      if (check_int(val, &testopt.midsockport)) {
        char tmpText[200];
        snprintf(tmpText, 200, "Invalid Middlebox test port number: %s", val);
        short_usage(name, tmpText);
      }
      continue;
    }
    
    else if (strncasecmp(key, "c2sport", 7) == 0) {
      if (check_int(val, &testopt.c2ssockport)) {
        char tmpText[200];
        snprintf(tmpText, 200, "Invalid C2S throughput test port number: %s", val);
        short_usage(name, tmpText);
      }
      continue;
    }
    
    else if (strncasecmp(key, "s2cport", 7) == 0) {
      if (check_int(val, &testopt.s2csockport)) {
        char tmpText[200];
        snprintf(tmpText, 200, "Invalid S2C throughput test port number: %s", val);
        short_usage(name, tmpText);
      }
      continue;
    }
    
    else {
      log_println(0, "Unrecognized config option: %s", key);
      exit(1);
    }
  }
  if (rc < 0) {
    log_println(0, "Syntax error in line %d of the config file %s", (-rc), ConfigFileName);
    exit(1);
  }
  fclose(conf);
}

/* This routine walks through the list of queued clients and kills off those
 * that don't respond.  New clients (after v3.5.5) will reqpond to this query
 * older clients wouldn't, so use the oldclient flag to tell them apart.
 * RAC 7/8/09
 */
void *
zombieWorker(void *head_ptr) {

    struct ndtchild *tmp_ptr, *tmp, *pre_ptr;
    int i=0, rc;
    struct timeval sel_tv;
    fd_set rfd;
    char tmpstr[8], buff[32];
    int msgType, msgLen=1;

    tmp_ptr = (struct ndtchild *)head_ptr;
    /* walk through this chain until the 4th client is reached, this helps
     * prevent cases where the current client finishes and the next one begins
     * before we are done looking
     * RAC 7/8/09
     */
    for (i=0; i<4; i++) {
	log_println(6, "Bumping queue pointer by 1 child=%d", tmp_ptr->pid);
	if (i == 3)
	    pre_ptr = tmp_ptr;
	tmp_ptr = tmp_ptr->next;
    }
    sprintf(tmpstr, "9990");
    i = 0;
    while (tmp_ptr != NULL) {
	if (tmp_ptr->oldclient == 0)  {
	    log_println(6, "old client found in queue, can't tell if it's a zombie, child=%d", tmp_ptr->pid);
	    tmp_ptr = tmp_ptr->next;
	    pre_ptr = pre_ptr->next;
	    continue;
	}
	log_println(6, "New client found, checking for response, child=%d", tmp_ptr->pid);
	send_msg(tmp_ptr->ctlsockfd, SRV_QUEUE, tmpstr, strlen(tmpstr));
	FD_ZERO(&rfd);
	FD_SET(tmp_ptr->ctlsockfd, &rfd);
	sel_tv.tv_sec = 1;
	sel_tv.tv_usec = 500000;
	rc = select((tmp_ptr->ctlsockfd)+1, &rfd, NULL, NULL, &sel_tv);
	switch (rc) {
	  case 0:
	    /*  a timeout occurred, remove zombie client from list */
	    log_println(6, "New client didn't respond - must be a zombie, get rid of it, child=%d", tmp_ptr->pid);
	    tmp = tmp_ptr;
	    pre_ptr->next = tmp_ptr->next;
	    tmp_ptr = tmp_ptr->next;
	    free(tmp);
	    i++;
	    break;
	  default:
	    log_println(6, "new client responded, bumping pointers child=%d", tmp_ptr->pid);
	    recv_msg(tmp_ptr->ctlsockfd, &msgType, buff, &msgLen);
	    tmp_ptr = tmp_ptr->next;
	    pre_ptr = pre_ptr->next;
	    break;
	  case -1:
	    log_println(6, "select returned errno=%d do nothing, child=%d", errno, tmp_ptr->pid);
	    break;
	}
    }
    zombie_check = i;
    return NULL;
}

void*
cputimeWorker(void* arg)
{
    char *logname = (char*) arg;
    FILE* file = fopen(logname, "w");
    struct tms buf;
    double start = secs();

    if (!file)
        return NULL;

    while (1) {
        if (!workerLoop) {
            break;
        }
        times(&buf);
        fprintf(file, "%.2f %ld %ld %ld %ld\n", secs() - start, buf.tms_utime, buf.tms_stime,
                buf.tms_cutime, buf.tms_cstime);
        fflush(file);
        mysleep(0.1);
    }

    fclose(file);

    return NULL;
}

void
run_test(web100_agent* agent, int ctlsockfd, TestOptions testopt, char *test_suite)
{

  char date[32];
  char spds[4][256];
  char logstr1[4096], logstr2[1024];
  char tmpstr[256];
  char isoTime[64];

  int n;
  int Timeouts, SumRTT, CountRTT, PktsRetrans, FastRetran, DataPktsOut;
  int AckPktsOut, CurrentMSS, DupAcksIn, AckPktsIn, MaxRwinRcvd, Sndbuf;
  int CurrentCwnd, SndLimTimeRwin, SndLimTimeCwnd, SndLimTimeSender, DataBytesOut;
  int SndLimTransRwin, SndLimTransCwnd, SndLimTransSender, MaxSsthresh;
  int CurrentRTO, CurrentRwinRcvd, MaxCwnd, CongestionSignals, PktsOut, MinRTT;
  int CongAvoid, CongestionOverCount, MaxRTT, OtherReductions, CurTimeoutCount;
  int AbruptTimeouts, SendStall, SlowStart, SubsequentTimeouts, ThruBytesAcked;
  int RcvWinScale, SndWinScale;
  int link=100, mismatch=0, bad_cable=0, half_duplex=0, congestion=0, totaltime;
  int ret, spd_index;
  int index, links[16], max, total;
  int c2sdata = 0, c2sack = 0, s2cdata = 0, s2cack = 0;
  int j;
  int totalcnt;
  int autotune;
  int dec_cnt, same_cnt, inc_cnt, timeout, dupack;
  int ifspeed;

  time_t stime;

  double rttsec, rwin, swin, cwin;
  double rwintime, cwndtime, sendtime;
  double oo_order, waitsec;
  double bw2, avgrtt, timesec, loss2, RTOidle;
  double s2cspd, c2sspd;
  double s2c2spd;
  double spd;
  double acks, aspd = 0, tmouts, rtran, dack;
  float runave[4];

  FILE *fp;

  web100_connection* conn;

  stime = time(0);
  log_println(4, "Child process %d started", getpid());

  for (spd_index=0; spd_index<4; spd_index++)
    for (ret=0; ret<256; ret++)
      spds[spd_index][ret] = 0x00;
  spd_index = 0;

  conn = web100_connection_from_socket(agent, ctlsockfd);
  autotune = web100_autotune(ctlsockfd, agent, conn);

  sprintf(buff, "v%s", VERSION);
  send_msg(ctlsockfd, MSG_LOGIN, buff, strlen(buff));

log_println(3, "run_test() routine, asking for test_suite = %s", test_suite);
  send_msg(ctlsockfd, MSG_LOGIN, test_suite, strlen(test_suite));
  /* if ((n = initialize_tests(ctlsockfd, &testopt, conn_options))) {
    log_println(0, "ERROR: Tests initialization failed (%d)", n);
    return;
  }
  */

  log_println(1, "Starting test suite:");
  if (testopt.midopt) {
    log_println(1, " > Middlebox test");
  }
  if (testopt.sfwopt) {
    log_println(1, " > Simple firewall test");
  }
  if (testopt.c2sopt) {
    log_println(1, " > C2S throughput test");
  }
  if (testopt.s2copt) {
    log_println(1, " > S2C throughput test");
  }
  
  alarm(15);
  if (test_mid(ctlsockfd, agent, &testopt, conn_options, &s2c2spd)) {
      log_println(0, "Middlebox test FAILED!");
      testopt.midopt = TOPT_DISABLED;
  }
  
  alarm(30);
  if (test_sfw_srv(ctlsockfd, agent, &testopt, conn_options)) {
      log_println(0, "Simple firewall test FAILED!");
      testopt.sfwopt = TOPT_DISABLED;
  }

  alarm(30);
  if (test_c2s(ctlsockfd, agent, &testopt, conn_options, &c2sspd, set_buff, window, autotune,
      device, &options, record_reverse, count_vars, spds, &spd_index)) {
      log_println(0, "C2S throughput test FAILED!");
      testopt.c2sopt = TOPT_DISABLED;
  }

  alarm(30);
  if (test_s2c(ctlsockfd, agent, &testopt, conn_options, &s2cspd, set_buff, window, autotune,
      device, &options, spds, &spd_index, count_vars, &peaks)) {
      log_println(0, "S2C throughput test FAILED!");
      testopt.s2copt = TOPT_DISABLED;
  }

  log_println(4, "Finished testing C2S = %0.2f Mbps, S2C = %0.2f Mbps", c2sspd/1000, s2cspd/1000);
  for (n=0; n<spd_index; n++) {
    sscanf(spds[n], "%d %d %d %d %d %d %d %d %d %d %d %d %f %d %d %d %d %d %d", &links[0],
        &links[1], &links[2], &links[3], &links[4], &links[5], &links[6],
        &links[7], &links[8], &links[9], &links[10], &links[11], &runave[n],
        &inc_cnt, &dec_cnt, &same_cnt, &timeout, &dupack, &ifspeed);
    max = 0;
    index = 0;
    total = 0;
    /* for (j=0; j<10; j++) { */
    if (ifspeed == -1)
	ifspeed = 10;
    for (j=0; j<=ifspeed; j++) {
      total += links[j];
      if (max < links[j]) {
        max = links[j];
        index = j;
      }
    }
    if (links[index] == -1)
      index = -1;
    fp = fopen(get_logfile(),"a");
    if (fp == NULL) {
      log_println(0, "Unable to open log file '%s', continuing on without logging", get_logfile());
    }
    else {
      fprintf(fp, "spds[%d] = '%s' max=%d [%0.2f%%]\n", n, spds[n],
          max, (float) max*100/total);
      fclose(fp);
    }

    /* When the C2S test is disabled, we have to skip the results */
    switch (n  + (testopt.c2sopt ? 0 : 2)) {
      case 0: c2sdata = index;
              log_print(1, "Client --> Server data detects link = ");
              break;
      case 1: c2sack = index;
              log_print(1, "Client <-- Server Ack's detect link = ");
              break;
      case 2: s2cdata = index;
              log_print(1, "Server --> Client data detects link = ");
              break;
      case 3: s2cack = index;
              log_print(1, "Server <-- Client Ack's detect link = ");
    }
    switch (index) {
      case -1: log_println(1, "System Fault");     break;
      case 0:  log_println(1, "RTT");              break;
      case 1:  log_println(1, "Dial-up");          break;
      case 2:  log_println(1, "T1");               break;
      case 3:  log_println(1, "Ethernet");         break;
      case 4:  log_println(1, "T3");               break;
      case 5:  log_println(1, "FastEthernet");     break;
      case 6:  log_println(1, "OC-12");            break;
      case 7:  log_println(1, "Gigabit Ethernet"); break;
      case 8:  log_println(1, "OC-48");            break;
      case 9:  log_println(1, "10 Gigabit Enet");  break;
      case 10: log_println(1, "Retransmissions");  break;
    }
  }

  /* get some web100 vars */
  if (options.cwndDecrease) {
    dec_cnt = inc_cnt = same_cnt = 0;
    CwndDecrease(agent, options.s2c_logname, &dec_cnt, &same_cnt, &inc_cnt);
    log_println(2, "####### decreases = %d, increases = %d, no change = %d", dec_cnt, inc_cnt, same_cnt);
  }

  web100_logvars(&Timeouts, &SumRTT, &CountRTT,
      &PktsRetrans, &FastRetran, &DataPktsOut,
      &AckPktsOut, &CurrentMSS, &DupAcksIn,
      &AckPktsIn, &MaxRwinRcvd, &Sndbuf,
      &CurrentCwnd, &SndLimTimeRwin, &SndLimTimeCwnd,
      &SndLimTimeSender, &DataBytesOut,
      &SndLimTransRwin, &SndLimTransCwnd,
      &SndLimTransSender, &MaxSsthresh,
      &CurrentRTO, &CurrentRwinRcvd, &MaxCwnd, &CongestionSignals,
      &PktsOut, &MinRTT, count_vars, &RcvWinScale, &SndWinScale,
      &CongAvoid, &CongestionOverCount, &MaxRTT, &OtherReductions,
      &CurTimeoutCount, &AbruptTimeouts, &SendStall, &SlowStart, 
      &SubsequentTimeouts, &ThruBytesAcked);

  /* if (rc == 0) { */
  /* Calculate some values */
  avgrtt = (double) SumRTT/CountRTT;
  rttsec = avgrtt * .001;
  /* loss = (double)(PktsRetrans- FastRetran)/(double)(DataPktsOut-AckPktsOut); */
  loss2 = (double)CongestionSignals/PktsOut;
  if (loss2 == 0) {
    if (c2sdata > 5)
      loss2 = .0000000001;  /* set to 10^-10 for links faster than FastE */
    else
      loss2 = .000001;    /* set to 10^-6 for now */
  }

  oo_order = (double)DupAcksIn/AckPktsIn;
  bw2 = (CurrentMSS / (rttsec * sqrt(loss2))) * 8 / 1024 / 1024;

  if ((SndWinScale > 15) || (Sndbuf < 65535))
    SndWinScale = 0;
  if ((RcvWinScale > 15) || (MaxRwinRcvd < 65535))
    RcvWinScale = 0;
  /* MaxRwinRcvd <<= RcvWinScale; */
  /* if ((SndWinScale > 0) && (RcvWinScale > 0))
   *    Sndbuf = (64 * 1024) << RcvWinScale;
   */
  /* MaxCwnd <<= RcvWinScale; */

  rwin = (double)MaxRwinRcvd * 8 / 1024 / 1024;
  swin = (double)Sndbuf * 8 / 1024 / 1024;
  cwin = (double)MaxCwnd * 8 / 1024 / 1024;

  totaltime = SndLimTimeRwin + SndLimTimeCwnd + SndLimTimeSender;
  rwintime = (double)SndLimTimeRwin/totaltime;
  cwndtime = (double)SndLimTimeCwnd/totaltime;
  sendtime = (double)SndLimTimeSender/totaltime;
  timesec = totaltime/1000000;
  RTOidle = (Timeouts * ((double)CurrentRTO/1000))/timesec;
  tmouts = (double)Timeouts / PktsOut;
  rtran = (double)PktsRetrans / PktsOut;
  acks = (double)AckPktsIn / PktsOut;
  dack = (double)DupAcksIn / (double)AckPktsIn;

  spd = ((double)DataBytesOut / (double)totaltime) * 8;
  waitsec = (double) (CurrentRTO * Timeouts)/1000;
  log_println(2, "CWND limited test = %0.2f while unlimited = %0.2f", s2c2spd, s2cspd);
  if ((s2c2spd > s2cspd) && (multiple == 0))
    log_println(2, "Better throughput when CWND is limited, may be duplex mismatch");
  else
    log_println(2, "Better throughput without CWND limits - normal operation");


  /* remove the following line when the new detection code is ready for release */
  old_mismatch = 1;
  if (old_mismatch == 1) {
    if ((cwndtime > .9) && (bw2 > 2) && (PktsRetrans/timesec > 2) &&
        (MaxSsthresh > 0) && (RTOidle > .01) && (link > 2) && (s2cspd < s2c2spd) &&
	(multiple == 0))
    {  if (s2cspd < c2sspd)
      mismatch = 1;
      else
        mismatch = 2;
      link = 0;
    }

    /* test for uplink with duplex mismatch condition */
    if (((s2cspd/1000) > 50) && (spd < 5) && (rwintime > .9) && (loss2 < .01)) {
      mismatch = 2;
      link = 0;
    }
  } else {
    /* This section of code is being held up for non-technical reasons.
     * Once those issues are resolved a new mismatch detection algorim 
     * will be placed here.
     *  RAC 5-11-06
     */
  }

  /* estimate is less than throughput, something is wrong */
  if (bw2 < spd)
    link = 0;

  if (((loss2*100)/timesec > 15) && (cwndtime/timesec > .6) &&
      (loss2 < .01) && (MaxSsthresh > 0))
    bad_cable = 1;

  /* test for Ethernet link (assume Fast E.) */
  if ((spd < 9.5) && (spd > 3.0) && ((s2cspd/1000) < 9.5) &&
      (loss2 < .01) && (oo_order < .035) && (link > 0))
    link = 10;

  /* test for wireless link */
  if ((sendtime == 0) && (spd < 5) && (bw2 > 50) &&
      ((SndLimTransRwin/SndLimTransCwnd) == 1) &&
      (rwintime > .90) && (link > 0))
    link = 3;

  /* test for DSL/Cable modem link */
  if ((SndLimTimeSender < 600) && (SndLimTransSender == 0) && (spd < 2) &&
      (spd < bw2) && (link > 0))
    link = 2;

  if ((rwintime > .95) && (SndLimTransRwin/timesec > 30) &&
      (SndLimTransSender/timesec > 30))
    half_duplex = 1;

  if ((cwndtime > .02) && (mismatch == 0) && ((cwin/rttsec) < (rwin/rttsec)))
    congestion = 1;

  sprintf(buff, "c2sData: %d\nc2sAck: %d\ns2cData: %d\ns2cAck: %d\n",
      c2sdata, c2sack, s2cdata, s2cack);
  send_msg(ctlsockfd, MSG_RESULTS, buff, strlen(buff));

  sprintf(buff, "half_duplex: %d\nlink: %d\ncongestion: %d\nbad_cable: %d\nmismatch: %d\nspd: %0.2f\n",
      half_duplex, link, congestion, bad_cable, mismatch, spd);
  send_msg(ctlsockfd, MSG_RESULTS, buff, strlen(buff));

  sprintf(buff, "bw: %0.2f\nloss: %0.9f\navgrtt: %0.2f\nwaitsec: %0.2f\ntimesec: %0.2f\norder: %0.4f\n",
      bw2, loss2, avgrtt, waitsec, timesec, oo_order);
  send_msg(ctlsockfd, MSG_RESULTS, buff, strlen(buff));

  sprintf(buff, "rwintime: %0.4f\nsendtime: %0.4f\ncwndtime: %0.4f\nrwin: %0.4f\nswin: %0.4f\n",
      rwintime, sendtime, cwndtime, rwin, swin);
  send_msg(ctlsockfd, MSG_RESULTS, buff, strlen(buff));

  sprintf(buff, "cwin: %0.4f\nrttsec: %0.6f\nSndbuf: %d\naspd: %0.5f\nCWND-Limited: %0.2f\n",
      cwin, rttsec, Sndbuf, aspd, s2c2spd);
  send_msg(ctlsockfd, MSG_RESULTS, buff, strlen(buff));

  sprintf(buff, "minCWNDpeak: %d\nmaxCWNDpeak: %d\nCWNDpeaks: %d\n", peaks.min, peaks.max, peaks.amount);
  send_msg(ctlsockfd, MSG_RESULTS, buff, strlen(buff));

  send_msg(ctlsockfd, MSG_LOGOUT, "", 0);

    sprintf(meta.date, "%s", get_ISOtime(isoTime));
    memcpy(meta.client_ip, rmt_host, strlen(rmt_host));
    memset(tmpstr, 0, 255);
    sprintf(tmpstr,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
        (int)s2c2spd,(int)s2cspd,(int)c2sspd, Timeouts, SumRTT, CountRTT, PktsRetrans,
        FastRetran, DataPktsOut, AckPktsOut, CurrentMSS, DupAcksIn, AckPktsIn);
    memcpy(meta.summary, tmpstr, strlen(tmpstr));
    memset(tmpstr, 0, 255);
    sprintf(tmpstr,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
        MaxRwinRcvd, Sndbuf, MaxCwnd, SndLimTimeRwin, SndLimTimeCwnd,
        SndLimTimeSender, DataBytesOut, SndLimTransRwin, SndLimTransCwnd,
        SndLimTransSender, MaxSsthresh, CurrentRTO, CurrentRwinRcvd);
    strncat(meta.summary, tmpstr, strlen(tmpstr));
    memset(tmpstr, 0, 255);
    sprintf(tmpstr,"%d,%d,%d,%d,%d",
        link, mismatch, bad_cable, half_duplex, congestion);
    strncat(meta.summary, tmpstr, strlen(tmpstr));
    memset(tmpstr, 0, 255);
    sprintf(tmpstr, ",%d,%d,%d,%d,%d,%d,%d,%d,%d", c2sdata, c2sack, s2cdata, s2cack,
        CongestionSignals, PktsOut, MinRTT, RcvWinScale, autotune);
    strncat(meta.summary, tmpstr, strlen(tmpstr));
    memset(tmpstr, 0, 255);
    sprintf(tmpstr, ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", CongAvoid, CongestionOverCount, MaxRTT, 
        OtherReductions, CurTimeoutCount, AbruptTimeouts, SendStall, SlowStart,
        SubsequentTimeouts, ThruBytesAcked);
    strncat(meta.summary, tmpstr, strlen(tmpstr));
    memset(tmpstr, 0, 255);
    sprintf(tmpstr, ",%d,%d,%d", peaks.min, peaks.max, peaks.amount);
    strncat(meta.summary, tmpstr, strlen(tmpstr));
    writeMeta();

  fp = fopen(get_logfile(),"a");
  if (fp == NULL) {
    log_println(0, "Unable to open log file '%s', continuing on without logging", get_logfile());
  }
  else {
    sprintf(date,"%15.15s", ctime(&stime)+4);
    fprintf(fp, "%s,", date);
    fprintf(fp,"%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
        rmt_host,(int)s2c2spd,(int)s2cspd,(int)c2sspd, Timeouts, SumRTT, CountRTT, PktsRetrans,
        FastRetran, DataPktsOut, AckPktsOut, CurrentMSS, DupAcksIn, AckPktsIn);
    fprintf(fp,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,",
        MaxRwinRcvd, Sndbuf, MaxCwnd, SndLimTimeRwin, SndLimTimeCwnd,
        SndLimTimeSender, DataBytesOut, SndLimTransRwin, SndLimTransCwnd,
        SndLimTransSender, MaxSsthresh, CurrentRTO, CurrentRwinRcvd);
    fprintf(fp,"%d,%d,%d,%d,%d",
        link, mismatch, bad_cable, half_duplex, congestion);
    fprintf(fp, ",%d,%d,%d,%d,%d,%d,%d,%d,%d", c2sdata, c2sack, s2cdata, s2cack,
        CongestionSignals, PktsOut, MinRTT, RcvWinScale, autotune);
    fprintf(fp, ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", CongAvoid, CongestionOverCount, MaxRTT, 
        OtherReductions, CurTimeoutCount, AbruptTimeouts, SendStall, SlowStart,
        SubsequentTimeouts, ThruBytesAcked);
    fprintf(fp, ",%d,%d,%d\n", peaks.min, peaks.max, peaks.amount);
    fclose(fp);
  }
  db_insert(spds, runave, cputimelog, options.s2c_logname, options.c2s_logname, testName, testPort, date,
          rmt_host, s2c2spd, s2cspd, c2sspd, Timeouts,
          SumRTT, CountRTT, PktsRetrans, FastRetran, DataPktsOut,
          AckPktsOut, CurrentMSS, DupAcksIn, AckPktsIn, MaxRwinRcvd,
          Sndbuf, MaxCwnd, SndLimTimeRwin, SndLimTimeCwnd, SndLimTimeSender,
          DataBytesOut, SndLimTransRwin, SndLimTransCwnd, SndLimTransSender,
          MaxSsthresh, CurrentRTO, CurrentRwinRcvd, link, mismatch,
          bad_cable, half_duplex, congestion, c2sdata, c2sack, s2cdata,
          s2cack, CongestionSignals, PktsOut, MinRTT, RcvWinScale,
          autotune, CongAvoid, CongestionOverCount, MaxRTT, OtherReductions,
          CurTimeoutCount, AbruptTimeouts, SendStall, SlowStart,
          SubsequentTimeouts, ThruBytesAcked, peaks.min, peaks.max, peaks.amount);
  if (usesyslog == 1) {
    sprintf(logstr1,"client_IP=%s,c2s_spd=%2.0f,s2c_spd=%2.0f,Timeouts=%d,SumRTT=%d,CountRTT=%d,PktsRetrans=%d,FastRetran=%d,DataPktsOut=%d,AckPktsOut=%d,CurrentMSS=%d,DupAcksIn=%d,AckPktsIn=%d,",
        rmt_host, s2cspd, c2sspd, Timeouts, SumRTT, CountRTT, PktsRetrans,
        FastRetran, DataPktsOut, AckPktsOut, CurrentMSS, DupAcksIn, AckPktsIn);
    sprintf(logstr2,"MaxRwinRcvd=%d,Sndbuf=%d,MaxCwnd=%d,SndLimTimeRwin=%d,SndLimTimeCwnd=%d,SndLimTimeSender=%d,DataBytesOut=%d,SndLimTransRwin=%d,SndLimTransCwnd=%d,SndLimTransSender=%d,MaxSsthresh=%d,CurrentRTO=%d,CurrentRwinRcvd=%d,",
        MaxRwinRcvd, Sndbuf, MaxCwnd, SndLimTimeRwin, SndLimTimeCwnd,
        SndLimTimeSender, DataBytesOut, SndLimTransRwin, SndLimTransCwnd,
        SndLimTransSender, MaxSsthresh, CurrentRTO, CurrentRwinRcvd);
    strcat(logstr1, logstr2);
    sprintf(logstr2,"link=%d,mismatch=%d,bad_cable=%d,half_duplex=%d,congestion=%d,c2sdata=%d,c2sack=%d,s2cdata=%d,s2cack=%d,CongestionSignals=%d,PktsOut=%d,MinRTT=%d,RcvWinScale=%d\n",
        link, mismatch, bad_cable, half_duplex, congestion,
        c2sdata, c2sack, s2cdata, s2cack,
        CongestionSignals, PktsOut, MinRTT, RcvWinScale);
    strcat(logstr1, logstr2);
    syslog(LOG_FACILITY|LOG_INFO, "%s", logstr1);
    closelog();
    log_println(4, "%s", logstr1);
  }
  if (testopt.s2copt) {
    close(testopt.s2csockfd);
  }

  /* If the admin view is turned on then the client process is going to update these
   * variables.  The need to be shipped back to the parent so the admin page can be
   * updated.  Otherwise the changes are lost when the client terminates.
   */
  if (admin_view == 1) {
    totalcnt = calculate(date, SumRTT, CountRTT, CongestionSignals, PktsOut, DupAcksIn, AckPktsIn,
        CurrentMSS, SndLimTimeRwin, SndLimTimeCwnd, SndLimTimeSender,
        MaxRwinRcvd, CurrentCwnd, Sndbuf, DataBytesOut, mismatch, bad_cable,
        (int)c2sspd, (int)s2cspd, c2sdata, s2cack, 1);
    gen_html((int)c2sspd, (int)s2cspd, MinRTT, PktsRetrans, Timeouts,
        Sndbuf, MaxRwinRcvd, CurrentCwnd, mismatch, bad_cable, totalcnt,
        refresh);
  }
  shutdown(ctlsockfd, SHUT_WR);
  /* shutdown(ctlsockfd, SHUT_RDWR); */
}

int
main(int argc, char** argv)
{
  int chld_pid, rc;
  int ctlsockfd = -1;
  int c, chld_pipe[2];
  int i, loopcnt, t_opts=0;
  struct sockaddr_storage cli_addr;
  struct sigaction new;
  web100_agent* agent;
  char *lbuf=NULL, *ctime();
  char buff[32], tmpstr[256];
  char test_suite[16];
  FILE *fp;
  size_t lbuf_max=0;
  fd_set rfd;
  struct timeval sel_tv;
  struct ndtchild *tmp_ptr = NULL, *new_child = NULL;
  time_t tt;
  socklen_t clilen;

  I2Addr listenaddr = NULL;
  int listenfd;
  char* srcname = NULL;
  char isoTime[64], dir[64];
  int debug = 0;
  options.limit = 0;
  options.snapDelay = 5;
  options.avoidSndBlockUp = 0;
  options.snaplog = 0;
  options.cwndDecrease = 0;
  memset(options.s2c_logname, 0, 128);
  memset(options.c2s_logname, 0, 128);
  peaks.min = -1;
  peaks.max = -1;
  peaks.amount = -1;
  DataDirName = NULL;

  memset(&testopt, 0, sizeof(testopt));

#ifdef AF_INET6
#define GETOPT_LONG_INET6(x) "46"x
#else
#define GETOPT_LONG_INET6(x) x
#endif

#ifdef EXPERIMENTAL_ENABLED
#define GETOPT_LONG_EXP(x) "y:"x
#else
#define GETOPT_LONG_EXP(x) x
#endif
  
  opterr = 0;
  while ((c = getopt_long(argc, argv,
          GETOPT_LONG_INET6(GETOPT_LONG_EXP("adhmoqrstvc:b:f:i:l:p:T:A:S:")), long_options, 0)) != -1) {
    switch (c) {
      case 'c':
        ConfigFileName = optarg;
        break;
      case 'd':
        debug++;
        break;
    }
  }

  log_init(argv[0], debug);
  
  if (ConfigFileName == NULL)
    ConfigFileName = CONFIGFILE;

  /*
   * Load Config file.
   * lbuf/lbuf_max keep track of a dynamically grown "line" buffer.
   * (It is grown using realloc.)
   * This will be used throughout all the config file reading and
   * should be free'd once all config files have been read.
   */

  opterr =  optind = 1;
  LoadConfig(argv[0], &lbuf, &lbuf_max);
  debug = 0;

  while ((c = getopt_long(argc, argv,
          GETOPT_LONG_INET6(GETOPT_LONG_EXP("adhmoqrstvc:b:f:i:l:p:T:A:S:")), long_options, 0)) != -1) {
    switch (c) {
      case '4':
        conn_options |= OPT_IPV4_ONLY;
        break;
      case '6':
        conn_options |= OPT_IPV6_ONLY;
        break;
      case 'r':
        record_reverse = 1;
        break;
      case 'h':
        srv_long_usage("ANL/Internet2 NDT version " VERSION " (server)");
        break;
      case 'v':
        printf("ANL/Internet2 NDT version %s (server)\n", VERSION);
        exit(0);
        break;
      case 'p':
        port = optarg;
        if (check_int(port, &testopt.mainport)) {
          char tmpText[200];
          snprintf(tmpText, 200, "Invalid primary port number: %s", optarg);
          short_usage(argv[0], tmpText);
        }
        break;
      case 302:
        if (check_int(optarg, &testopt.midsockport)) {
          char tmpText[200];
          snprintf(tmpText, 200, "Invalid Middlebox test port number: %s", optarg);
          short_usage(argv[0], tmpText);
        }
        break;
      case 303:
        if (check_int(optarg, &testopt.c2ssockport)) {
          char tmpText[200];
          snprintf(tmpText, 200, "Invalid C2S throughput test port number: %s", optarg);
          short_usage(argv[0], tmpText);
        }
        break;
      case 304:
        if (check_int(optarg, &testopt.s2csockport)) {
          char tmpText[200];
          snprintf(tmpText, 200, "Invalid S2C throughput test port number: %s", optarg);
          short_usage(argv[0], tmpText);
        }
        break;
      case 'a':
        admin_view = 1;
        break;
      case 'f':
        VarFileName = optarg;
        break;
      case 'i':
        device = optarg;
        break;
      case 'l':
        set_logfile(optarg);
        break;
      case 'o':
        old_mismatch = 1;
        break;
      case 301:
        if (mrange_parse(optarg)) {
          char tmpText[300];
          snprintf(tmpText, 300, "Invalid range: %s", optarg);
          short_usage(argv[0], tmpText);
        }
      case 'm':
        multiple = 1;
        break;
      case 'x':
        max_clients = atoi(optarg);
        break;
      case 'q':
        queue = 0;
        break;
      case 's':
        usesyslog = 1;
        break;
      case 't':
        dumptrace = 1;
        break;
      case 'b':
        if (check_int(optarg, &window)) {
          char tmpText[200];
          snprintf(tmpText, 200, "Invalid window size: %s", optarg);
          short_usage(argv[0], tmpText);
        }
        set_buff = 1;
        break;
      case 'd':
        debug++;
        break;
      case 305:
        options.snapDelay = atoi(optarg);
        break;
      case 'y':
        options.limit = atoi(optarg);
        break;
      case 306:
        options.avoidSndBlockUp = 1;
        break;
      case 308:
        options.cwndDecrease = 1;
      case 307:
        options.snaplog = 1;
        break;
      case 309:
        cputime = 1;
        break;
      case 310:
        useDB = 1;
        break;
      case 311:
        dbDSN = optarg;
        break;
      case 312:
        dbUID = optarg;
        break;
      case 313:
        dbPWD = optarg;
        break;
      case 'T':
        refresh = atoi(optarg);
        break;
      case 'A':
        AdminFileName = optarg;
        break;
      case 'L':
        DataDirName = optarg;
        break;
      case 'S':
        SysLogFacility = optarg;
        break;
      case '?':
        short_usage(argv[0], "");
        break;
    }
  }

  if (optind < argc) {
    short_usage(argv[0], "Unrecognized non-option elements");
  }

  if (debug > get_debuglvl()) {
    set_debuglvl(debug);
  }
  
  testopt.multiple = multiple;
  
  /* First check to see if program is running as root.  If not, then warn
   * the user that link type detection is suspect.  Then downgrade effective
   * userid to non-root user until needed.
   */
  if (getuid() != 0) {
    log_print(0, "Warning: This program must be run as root to enable the Link Type");
    log_println(0, " detection algorithm.\nContinuing execution without this algorithm");
  }

  if (VarFileName == NULL) {
    sprintf(wvfn, "%s/%s", BASEDIR, WEB100_FILE);
    VarFileName = wvfn;
  }
  
  if (AdminFileName == NULL) {
    sprintf(apfn, "%s/%s", BASEDIR, ADMINFILE);
    AdminFileName = apfn;
  }

  if (DataDirName == NULL) {
    sprintf(logd, "%s/%s/", BASEDIR, LOGDIR);
    DataDirName = logd;
  }
  else if (DataDirName[0] != '/') {
    sprintf(logd, "%s/%s/", BASEDIR, DataDirName);
    DataDirName = logd;
  }

  if (SysLogFacility != NULL) {
    i = 0;
    while (facilitynames[i].c_name) {
      if (strcmp(facilitynames[i].c_name, SysLogFacility) == 0) {
        syslogfacility = facilitynames[i].c_val;
        break;
      }
      ++i;
    }
    if (facilitynames[i].c_name == NULL) {
      log_println(0, "Warning: Unknown syslog facility [%s] --> using default (%d)",
          SysLogFacility, syslogfacility);
      SysLogFacility = NULL;
    }
  }
  
  log_println(1, "ANL/Internet2 NDT ver %s", VERSION);
  log_println(1, "\tVariables file = %s\n\tlog file = %s", VarFileName, get_logfile());
  if (admin_view) {
    log_println(1, "\tAdmin file = %s", AdminFileName);
  }
  if (usesyslog) {
    log_println(1, "\tSyslog facility = %s (%d)", SysLogFacility ? SysLogFacility : "default", syslogfacility);
  }
  log_println(1, "\tDebug level set to %d", debug);

  initialize_db(useDB, dbDSN, dbUID, dbPWD);

  memset(&new, 0, sizeof(new));
  new.sa_handler = cleanup;

  /* Grab all signals and run them through my cleanup routine.  2/24/05 */
  for (i=1; i<32; i++) {
    if ((i == SIGKILL) || (i == SIGSTOP))
      continue;    /* these signals can't be caught */
    sigaction(i, &new, NULL);
  }

  /*
   * Bind our local address so that the client can send to us.
   */
  if (srcname && !(listenaddr = I2AddrByNode(get_errhandle(), srcname))) {
    err_sys("server: Invalid source address specified");
  }
  if ((listenaddr = CreateListenSocket(listenaddr, port, conn_options, 
					((set_buff > 0)? window : 0))) == NULL) {
    err_sys("server: CreateListenSocket failed");
  }
  listenfd = I2AddrFD(listenaddr);

  log_println(1, "server ready on port %s", port);

  /* Initialize Web100 structures */
  count_vars = web100_init(VarFileName);

  /* The administrator view automatically generates a usage page for the
   * NDT server.  This page is then accessable to the general public.
   * At this point read the existing log file and generate the necessary
   * data.  This data is then updated at the end of each test.
   * RAC 11/28/03
   */
  if (admin_view == 1)
    view_init(refresh);

  /* Get the server's metadata info (OS name and kernel version
   * RAC 7/7/09
   */
  if ((fp = fopen("/proc/sys/kernel/hostname", "r")) == NULL)
    log_println(0, "Unable to determine server Hostname.");
  else {
    fscanf(fp, "%s", meta.server_name);
    fclose(fp);
  }
  if ((fp = fopen("/proc/sys/kernel/osrelease", "r")) == NULL)
    log_println(0, "Unable to determine server OS Version.");
  else {
    fscanf(fp, "%s", meta.server_os);
    fclose(fp);
  }
  

  /* create a log file entry every time the web100srv process starts up. */
  ndtpid = getpid();
  tt = time(0);
  fp = fopen(get_logfile(),"a");
  if (fp == NULL) {
    log_println(0, "Unable to open log file '%s', continuing on without logging", get_logfile());
  }
  else {
    fprintf(fp, "Web100srv (ver %s) process (%d) started %15.15s\n",
        VERSION, ndtpid, ctime(&tt)+4);
    fclose(fp);
  }
  if (usesyslog == 1)
    syslog(LOG_FACILITY|LOG_INFO, "Web100srv (ver %s) process started",
        VERSION);

  /* scan through the interface device list and get the names/speeds of each
   * if.  The speed data can be used to cap the search for the bottleneck link
   * capacity.  The intent is to reduce the impact of interrupt coalescing on 
   * the bottleneck link detection algorithm
   * RAC 7/14/09
   */
  get_iflist();

  for (i=0; iflist.speed[i]>0; i++)
     log_println(4, "Generated iflist with device=%s and if_speed=%d", iflist.name[i], iflist.speed[i]);

  /*
   * Wait at accept() for a new connection from a client process.
   */

  /* These 2 flags keep track of running processes.  The 'testing' flag
   * indicated a test is currently being performed.  The 'waiting' counter
   * shows how many tests are pending.
   * Rich Carlson 3/11/04
   */

  testing = 0;
  waiting = 0;
  loopcnt = 0;
  head_ptr = NULL;
  sig17 = 0;
  for(;;){
    int i;
    char *name;

    if (head_ptr == NULL)
      log_println(3, "nothing in queue");
    else
      log_println(3, "Queue pointer = %d, testing = %d, waiting = %d, zombie_check = %d",
		head_ptr->pid, testing, waiting, zombie_check);

    if (sig17 == 1) {
      child_sig();
      sig17 = 0;
    }

    FD_ZERO(&rfd);
    FD_SET(listenfd, &rfd);
    if (waiting > 0) {
      sel_tv.tv_sec = 3;
      sel_tv.tv_usec = 0;
      log_println(3, "Waiting for new connection, timer running");
      rc = select(listenfd+1, &rfd, NULL, NULL, &sel_tv);
      tt = time(0);
      if (head_ptr != NULL) {
        log_println(3, "now = %ld Process started at %ld, run time = %ld",
            tt, head_ptr->stime, (tt - head_ptr->stime));
        if (tt - head_ptr->stime > 90) {
          /* process is stuck at the front of the queue. */
          fp = fopen(get_logfile(),"a");
          if (fp != NULL) {
            fprintf(fp, "%d children waiting in queue: Killing off stuck process %d at %15.15s\n", 
                waiting, head_ptr->pid, ctime(&tt)+4);
            fclose(fp);
          }
          tmp_ptr = head_ptr->next;
          kill(head_ptr->pid, SIGKILL);
          free(head_ptr);
          head_ptr = tmp_ptr;
          testing = 0;
          if (waiting > 0)
            waiting--;
        }
      }
    }
    else {
      log_println(3, "Timer not running, waiting for new connection");
      rc = select(listenfd+1, &rfd, NULL, NULL, NULL);
    }

    if (rc < 0) {
      log_println(5, "Select exited with rc = %d", rc);
      continue;
    }

    if (rc == 0) {    /* select exited due to timer expired */
      log_println(3, "Timer expired while waiting for a new connection");
      if (multiple == 0) {
        if ((waiting > 0) && (testing == 0))
          goto ChldRdy;
      }
      continue;
    }
    else {
      if (multiple == 0) {
        if ((waiting > 0) && (testing == 0))
          goto ChldRdy;
        log_println(3, "New connection received, waiting for accept() to complete");
      }
      clilen = sizeof(cli_addr);
      ctlsockfd = accept(listenfd, (struct sockaddr *) &cli_addr, &clilen);
      {
        size_t tmpstrlen = sizeof(tmpstr);
        I2Addr tmp_addr = I2AddrBySockFD(get_errhandle(), ctlsockfd, False);
        I2AddrNodeName(tmp_addr, tmpstr, &tmpstrlen);
        I2AddrFree(tmp_addr);
      }
      log_println(4, "New connection received from [%s].", tmpstr);
      if (ctlsockfd < 0) {
        if (errno == EINTR)
          continue; /*sig child */
        perror("Web100srv server: accept error");
        continue;
      }
      /* the specially crafted data that kicks off the old clients */
      write(ctlsockfd, "123456 654321", 13);
      new_child = (struct ndtchild *) malloc(sizeof(struct ndtchild));
      tt = time(0);
      name = tmpstr;
      rmt_host = tmpstr;

      /* At this point we have received a connection from a client, meaning that
       * a test is being requested.  At this point we should apply any policy 
       * or AAA functions to the incoming connection.  If we don't like the
       * client, we can refuse the connection and loop back to the begining.
       * There would need to be some additional logic installed if this AAA
       * test relied on more than the client's IP address.  The client would
       * also require modification to allow more credentials to be created/passed
       * between the user and this application.
       */
    }

    pipe(chld_pipe);
    chld_pid = fork();

    switch (chld_pid) {
      case -1:    /* an error occured, log it and quit */
        log_println(0, "fork() failed, errno = %d", errno);
        break;
      default:    /* this is the parent process, handle scheduling and
                   * queuing of multiple incoming clients
                   */
        log_println(5, "Parent process spawned child = %d", chld_pid);
        log_println(5, "Parent thinks pipe() returned fd0=%d, fd1=%d", chld_pipe[0], chld_pipe[1]);

        close(chld_pipe[0]);
/*  Move code below new_child struct filling
 *  if (multiple == 1)
 *           goto multi_client;
 */

        /* Check to see if we have more than max_clients waiting in the queue
         * If so, tell them to go away.
         * changed for M-Lab deployment  1/28/09  RAC
         */
        if (waiting > max_clients) {
          log_println(0, "Too many clients waiting to be served, Please try again later.");
          sprintf(tmpstr, "9988");
          send_msg(ctlsockfd, SRV_QUEUE, tmpstr, strlen(tmpstr));
          kill(new_child->pid, SIGKILL);
          free(new_child);
          continue;
        }

	t_opts = initialize_tests(ctlsockfd, &testopt, test_suite);
        new_child->pid = chld_pid;
        strncpy(new_child->addr, rmt_host, strlen(rmt_host));
        strncpy(new_child->host, name, strlen(name));
        new_child->stime = tt + (waiting*45);
        new_child->qtime = tt;
        new_child->pipe = chld_pipe[1];
        new_child->ctlsockfd = ctlsockfd;
	if (t_opts & TEST_STATUS)
	    new_child->oldclient = 1;
	else
	    new_child->oldclient = 0;
	memset(new_child->tests, 0, sizeof(test_suite));
	memcpy(new_child->tests, test_suite, strlen(test_suite));
        new_child->next = NULL;

log_println(3, "initialize_tests returned old/new client = %d, test_suite = %s", new_child->oldclient, new_child->tests);

        if (multiple == 1)
          goto multi_client;

        if ((testing == 1) && (queue == 0)) {
          log_println(3, "queuing disabled and testing in progress, tell client no");
          send_msg(ctlsockfd, SRV_QUEUE, "9999", 4);
          free(new_child);
          continue;
        }

        /* Check to see if we have more than max_clients waiting in the queue
         * If so, tell them to go away.
         * changed for M-Lab deployment  1/28/09  RAC
         */
/*        if (waiting > max_clients) {
          log_println(0, "Too many clients waiting to be served, Please try again later.");
          sprintf(tmpstr, "9988");
          send_msg(ctlsockfd, SRV_QUEUE, tmpstr, strlen(tmpstr));
          kill(new_child->pid, SIGKILL);
          free(new_child);
          continue;
        }
*/

        waiting++;
        log_println(5, "Incrementing waiting variable now = %d", waiting);
        if (head_ptr == NULL)
          head_ptr = new_child;
        else {
          log_println(4, "New request has arrived, adding request to queue list");
          tmp_ptr = head_ptr;
          while (tmp_ptr->next != NULL)
            tmp_ptr = tmp_ptr->next;
          tmp_ptr->next = new_child;

          if (get_debuglvl() > 3) {
            log_println(4, "Walking scheduling queue");
            tmp_ptr = head_ptr;
            while (tmp_ptr != NULL) {
              log_println(4, "\tChild %d, host: %s [%s], next=0x%x", tmp_ptr->pid,
                  tmp_ptr->host, tmp_ptr->addr, (u_int64_t) tmp_ptr->next);
              if (tmp_ptr->next == NULL)
                break;
              tmp_ptr = tmp_ptr->next;
            }
          }
        }

        /* At this point send a message to the client via the ctlsockfd
         * saying that N clients are waiting in the queue & testing will
         * begin within Nx60 seconds.  Only if (waiting > 0)
         * Clients who leave will be handled in the run_test routine.
         */
        if (waiting > 1) {
          log_println(3, "%d clients waiting, telling client %d testing will begin within %d minutes",
              (waiting-1), tmp_ptr->pid, (waiting-1));

          sprintf(tmpstr, "%d", (waiting-1));
          send_msg(ctlsockfd, SRV_QUEUE, tmpstr, strlen(tmpstr));
        }
	  
        if (testing == 1) 
          continue;

ChldRdy:
        testing = 1;

        /* update all the clients so they have some idea that progress is occurring */
        if (waiting > 1) {
          i = waiting - 1;
          tmp_ptr = head_ptr->next;
	  if (tmp_ptr == NULL)
	      waiting = 1;
          while (tmp_ptr != NULL) {
            log_println(3, "%d clients waiting, updating client %d testing will begin within %d minutes",
                (waiting-1), tmp_ptr->pid, (waiting-i));

            sprintf(tmpstr, "%d", (waiting-i));
            send_msg(tmp_ptr->ctlsockfd, SRV_QUEUE, tmpstr, strlen(tmpstr));
            tmp_ptr = tmp_ptr->next;
            i--;
          }
	  if ((waiting > 5) && (zombie_check == 0)) {
	    zombie_check = -1;
	    log_println(4, "More than 5 clients in the queue, remove zombies");
              if (pthread_create(&zombieThreadId, NULL, zombieWorker, (void *)head_ptr)) {
		log_println(0, "Cannot create thread to kill off zonbie clients!");
		zombie_check = 0;
	      }
	      if (zombie_check > 0) {
		waiting -= zombie_check;
		zombie_check = 0;
	      }
	  }
	
        }
        log_println(3, "Telling client %d testing will begin now", head_ptr->pid);

        /* at this point we have successfully set everything up and are ready to
         * start testing.  The write() on the control socket tells the client
         * that the applet should drop out of the wait loop and get ready to begin
         * testing.  The write() on the pipe tells the child process it's at the
         * head of the queue, or it's in multi-client mode, and its OK for this 
         * client process to wake up and begin testing.
         *
         * This is the point in time where we can/should contact any meta
         * scheduler to verify that it is OK to really start a test.  If
         * we are sharing the server host with other applications, and we
         * want unfettered access to the link, now is the time to make this
         * request.
         */

        head_ptr->stime = time(0);
multi_client:
	memset(tmpstr, 0, sizeof(tmpstr));
        if (multiple == 1) {
          sprintf(tmpstr, "go %d %s", t_opts, test_suite);
          send_msg(ctlsockfd, SRV_QUEUE, "0", 1);
          write(chld_pipe[1], tmpstr, strlen(tmpstr));
          close(chld_pipe[1]);
          close(ctlsockfd);
        }
        else {
          sprintf(tmpstr, "go %d %s", t_opts, head_ptr->tests);
          send_msg(head_ptr->ctlsockfd, SRV_QUEUE, "0", 1);
          write(head_ptr->pipe, tmpstr, strlen(tmpstr));
          close(head_ptr->pipe);
          close(head_ptr->ctlsockfd);
        }
        continue;
        break;
      case 0:    /* this is the child process, it handles
                  * the testing function 
                  */
        log_println(4, "Child thinks pipe() returned fd0=%d, fd1=%d for pid=%d",
            chld_pipe[0], chld_pipe[1], chld_pid);
        close(listenfd);
        close(chld_pipe[1]);
        if ((agent = web100_attach(WEB100_AGENT_TYPE_LOCAL, NULL)) == NULL) {
          web100_perror("web100_attach");
          return 1;
        }

        /* This is the child process from the above fork().  The parent
         * is in control, and will send this child a signal when it gets
         * to the head of the testing queue.  Post a read() and simply
         * wait for the parent to let us know it's time to move on.
         * Rich Carlson 3/11/04
         */
        for (;;) {
	  memset(buff, 0, sizeof(buff));
          read(chld_pipe[0], buff, 32);
          if (strncmp(buff, "go", 2) == 0) {
            log_println(6, "Got 'go' signal from parent, ready to start testing");
            break;
          }
        }

        set_timestamp();

	/* The next 3 instructions retrieve the passed in values for the tests to run.
	 * t_opts will be the bitmapped version of this data and test_suite will be a 
	 * character string with the same info.  Note: this assumes that the structure
	 * of buff is fixed with the character strings starting in these locations.
	 * RAC 7/8/09
	 */
	memset(test_suite, 0, 16);
	t_opts = atoi(buff+3);
	memcpy(test_suite, buff+5, (strlen(buff)-5));
	/* memcpy(test_suite, buff+6, 7); */
	log_println(5, "extracting test_suite '%s' and t_opts '%x' from buff '%s'", test_suite, t_opts, buff);

        {
            I2Addr tmp_addr = I2AddrBySockFD(get_errhandle(), ctlsockfd, False);
            testPort = I2AddrPort(tmp_addr);
	    meta.ctl_port = testPort;
            snprintf(testName, 250, "%s", name);
            I2AddrFree(tmp_addr);
            memset(cputimelog, 0, 256);
            if (cputime) {
		strncpy(cputimelog, DataDirName, strlen(DataDirName));
		if ((opendir(cputimelog) == NULL) && (errno == ENOENT))
		    mkdir(cputimelog, 0755);
		get_YYYY(dir);
		strncat(cputimelog, dir, 4);
		if ((opendir(cputimelog) == NULL) && (errno == ENOENT))
		    mkdir(cputimelog, 0755);
		strncat(cputimelog, "/", 1);
		get_MM(dir);
		strncat(cputimelog, dir, 2);
		if ((opendir(cputimelog) == NULL) && (errno == ENOENT))
		    mkdir(cputimelog, 0755);
		strncat(cputimelog, "/", 1);
		get_DD(dir);
		strncat(cputimelog, dir, 2);
		if ((opendir(cputimelog) == NULL) && (errno == ENOENT))
		    mkdir(cputimelog, 0755);
		strncat(cputimelog, "/", 1);
		sprintf(dir, "%s_%s:%d.cputime", get_ISOtime(isoTime), name, testPort);
		strncat(cputimelog, dir, strlen(dir));
		memcpy(meta.CPU_time, dir, strlen(dir));
              if (pthread_create(&workerThreadId, NULL, cputimeWorker, (void*) cputimelog)) {
                  log_println(0, "Cannot create worker thread for writing cpu usage!");
                  workerThreadId = 0;
                  memset(cputimelog, 0, 256);
              }
            }
        }
        /* write the incoming connection data into the log file */
        fp = fopen(get_logfile(),"a");
        if (fp == NULL) {
          log_println(0, "Unable to open log file '%s', continuing on without logging", get_logfile());
        }
        else {
          fprintf(fp,"%15.15s  %s port %d\n",
              ctime(&tt)+4, name, testPort);
          if (cputime && workerThreadId) {
              log_println(1, "cputime trace file: %s\n", cputimelog);
              fprintf(fp, "cputime trace file: %s\n", cputimelog);
          }
          fclose(fp);
        }
        close(chld_pipe[0]);
	if (t_opts & TEST_MID)
	    testopt.midopt = TOPT_ENABLED;
	if (t_opts & TEST_SFW)
	    testopt.sfwopt = TOPT_ENABLED;
	if (t_opts & TEST_C2S)
	    testopt.c2sopt = TOPT_ENABLED;
	if (t_opts & TEST_S2C)
	    testopt.s2copt = TOPT_ENABLED;
        alarm(30);  /* die in 30 seconds, but only if a test doesn't get started 
                     * reset alarm() before every test */

        run_test(agent, ctlsockfd, testopt, test_suite);

        log_println(3, "Successfully returned from run_test() routine");
        close(ctlsockfd);
        web100_detach(agent);

        if (cputime && workerThreadId) {
            workerLoop = 0;
            pthread_join(workerThreadId, NULL);
        }

        exit(0);
        break;
    }
  }
}
