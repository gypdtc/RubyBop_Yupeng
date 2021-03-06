#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <semaphore.h> //BOP_msg locking
#include <fcntl.h>

#include <unistd.h>
//#include <sys/siginfo.h>

#include <sys/ucontext.h>
#include <execinfo.h>

#include "bop_api.h"
#include "bop_ports.h"
#include "bop_ppr_sync.h"
#include "utils.h"
#include "ipa_hooks.h"
#include "ipa.h"

#define OWN_GROUP() if (setpgid(0, 0) != 0) {    perror("setpgid");     exit(-1);  }

extern bop_port_t bop_io_port;
extern bop_port_t bop_merge_port;
extern bop_port_t postwait_port;
extern bop_port_t bop_ordered_port;
extern bop_port_t bop_alloc_port;

volatile task_status_t task_status = SEQ;
volatile ppr_pos_t ppr_pos = GAP;
bop_mode_t bop_mode = PARALLEL;
int spec_order = 0;
int ppr_index = 0;
static int ppr_static_id;

stats_t bop_stats = { 0 };

static int bopgroup;

int monitor_process_id = 0;
int monitor_group = 0; //the process group that PPR tasks are using
static bool is_monitoring = false;
static bool errored = false;

static void __attribute__((noreturn)) wait_process(void);
static void __attribute__((noreturn)) end_clean(void); //exit if children errored or call abort
static int  cleanup_children(void); //returns the value that end_clean would call with _exit (or 0 if would have aborted)
void SigBopExit( int signo );
static void block_wait(void);
static void unblock_wait(void);
//exec pipe

//Semaphore msg init
extern void msg_init();
extern void on_main_complete( void );


static void _ppr_group_init( void ) {
  bop_msg( 3, "task group starts (gs %d)", BOP_get_group_size() );

  /* setup bop group id.*/
  bopgroup = getpid();
  assert( getpgrp() == -monitor_group );
  spec_order = 0;
  ppr_sync_data_reset( );
  ppr_group_init( );
  task_status = MAIN;  // at the end so monitoring is disabled until this point
}

/* Called at the ppr start of a main task and the end of the prior ppr
   of a spec task. */
static void _ppr_task_init( void ) {
  switch (task_status) {
  case MAIN:
    assert( spec_order == 0);
    break;
  case SPEC:
    spec_order += 1;
    break;
  case UNDY:
  case SEQ:
    assert(0);
  }

  bop_msg( 2, "ppr task %d (%d total) starts. pgrp = %d", spec_order, BOP_get_group_size(), getpgrp() );

  //assert( setpgid(0, bopgroup)==0 );
  assert (getpgrp() == -monitor_group);
  ppr_task_init( );
}

int _BOP_ppr_begin(int id) {
  ppr_pos_t old_pos = ppr_pos;
  ppr_pos = PPR;
  ppr_index ++;

  switch (task_status) {
  case UNDY:
    return 0;
  case SEQ:
    _ppr_group_init( );

    /* NO BREAK. either the ppr task was started by SPEC earlier or
       now it is SPEC continuing into the next PPR region */
  case SPEC:
    if (task_status == SPEC && old_pos != GAP) {
      bop_msg(2, "Nested begin PPR (region %d inside region %d) ignored", id, ppr_static_id);
      return 0;
    }

    ppr_static_id = id;

    /* no spawn if sequential or reaching group size */
    if (bop_mode == SERIAL || spec_order >= partial_group_get_size( ) - 1) {
      if (bop_mode == SERIAL) _ppr_task_init( );
      return 0;
    }
    block_wait();
    int fid = fork( );
    unblock_wait();
    if (fid == -1) {
      bop_msg (2, "OS unable to fork more tasks" );
      if ( task_status == MAIN) {
      	task_status = SEQ;
      	bop_mode = SERIAL;
      }
      else
	     partial_group_set_size( spec_order + 1 );

      return 0;
    }

    if (fid > 0) { /* main or senior spec continues */
      if (task_status == MAIN) _ppr_task_init( );
      return 0;
    }

    /* new spec task */
    task_status = SPEC;
    ppr_pos = GAP;
    _ppr_task_init( );
    return 1;

  case MAIN:
    bop_msg(2, "Nested begin PPR (region %d inside region %d) ignored", id, ppr_static_id);
    return 0;

  default:
    assert(0);
  }
  return 0;
}

/* Not supported because of too many cases we have to worry depending
   on when this called.  Also there is no need for this call.
   BOP_abort_spec should suffice.  In any case, sending SIGUSR2 will
   break the system unless UNDY is running. */

// void BOP_abort_spec_group( char *msg ) {
//  bop_msg(2, "Abort all speculation because %s", msg );
//  kill( 0, SIGUSR2 );
// }
bool waiting = false;
void temp_sigint(int sigo){
  waiting = true;
}
void error_alert_monitor(){
  kill(monitor_process_id, SIGUSR2);
}
extern void io_on_malloc_rescue(void);
int spawn_undy(void);
extern void print_headers(void);
//if malloc cannot meet a request, it calls this funcion
void BOP_malloc_rescue(char * msg, size_t size){
  bop_msg(2, "Malloc rescue begin. aligned size & header: %u Failure: %s", size, msg);
  // print_headers();
  if(task_status == SEQ || task_status == UNDY || bop_mode == SERIAL){
    bop_msg(1, "ERROR. Malloc failed while logically sequential");
  }else if(task_status == MAIN || (task_status == SPEC && spec_order == 0)){
      bop_msg(3, "Changing pid %d (mode %s)", getpid(),
          task_status == MAIN ? "Main" : "SPEC");
      //Hangs here... io_on_malloc_rescue(); //before anything changes, but something will change
      // //'undy wins the race'
      int now_undy = spawn_undy();
      if(!now_undy){
        //die
        bop_msg(1, "Aborting malloc failing proc.");
        _exit(0);
      }else{
        bop_msg(1, "New saved undy returning.");
        return;
      }
      return;//user-process
  }else if(task_status == SPEC && spec_order > 0){
      bop_msg(3, "Unable to save later SPEC tasks. Exiting");
      _exit(0);
  }else{
    BOP_abort_spec("Didn't know how to process BOP_malloc_rescue");
    _exit(0); //for exit!
  }
  _exit(0); //my sanity
}
void  __attribute__ ((format (printf, 1, 2))) BOP_abort_spec(const char* msg, ...){
  if (task_status == SEQ
      || task_status == UNDY || bop_mode == SERIAL)
    return;
    va_list argptr;
     va_start(argptr,msg);
  if (task_status == MAIN)  { /* non-mergeable actions have happened */
    if ( partial_group_get_size() > 1 ) {
      bop_msg(2, "Abort main speculation because %s", msg, argptr);
      partial_group_set_size( 1 );
    }
  }else{
    bop_msg(2, "Abort alt speculation because %s", msg, argptr);
    partial_group_set_size( spec_order );
    signal_commit_done( );
    end_clean(); //_exit(0);  /* die silently, but reap children*/
  }
}
void BOP_abort_next_spec( char *msg ) {
  if (task_status == SEQ
      || task_status == UNDY || bop_mode == SERIAL)
    return;

  bop_msg(2, "Abort next speculation because %s", msg);
  if (task_status == MAIN)  /* non-mergeable actions have happened */
    partial_group_set_size( 1 );
  else
    partial_group_set_size( spec_order + 1 );
}

static int undy_ppr_count;

void post_ppr_undy( void ) {
  undy_ppr_count ++;

  if (undy_ppr_count < partial_group_get_size() - 1) {
    bop_msg(3,"Undy reaches EndPPR %d times", undy_ppr_count);
    return;
  }

  /* Ladies and Gent: This is the finish line.  If understudy lives to block
     off SIGUSR2 (without being aborted by it before), then it wins the race
     (and thumb down for parallelism).*/
  bop_msg(3,"Understudy finishes and wins the race");

  // indicate the success of the understudy
  kill(0, SIGUSR2);
  kill(-monitor_group, SIGUSR1); //TODO why is this here????? main requires a special signal?

  undy_succ_fini( );

  task_status = SEQ;
  ppr_pos = GAP;
  bop_stats.num_by_undy += undy_ppr_count;
}

/* Return true if it is UNDY (or SEQ in the rare case). */
int spawn_undy( void ) {
  pid_t caller = getpid();
  block_wait();
  int fid = fork( );
  unblock_wait();
  switch( fid ) {
  case -1:
    bop_msg(3,"OS cannot fork more process.");
    /* Turn main into the understudy and effectively abort all
       speculation (by not committing).*/
    task_status = SEQ;
    bop_mode = SERIAL;
    return TRUE;
  case 0:
    task_status = UNDY;
    ppr_pos = GAP;
    spec_order = -1;
    //assert( setpgid(0, bopgroup) == 0 );
    assert (getpgrp() == -monitor_group);
    bop_msg(3,"Understudy starts pid %d pgrp %d parent: %d", getpid(), getpgrp(), caller);

    signal_undy_created( fid );

    bop_stats.num_by_main += 1;
    undy_ppr_count = 0;

    undy_init( );

    return TRUE;
  default:
    return FALSE;
  }
}
void undy_on_create(){

}
/* It won't return if not correct. */
static void _ppr_check_correctness( void ) {
  if ( task_status != MAIN )
    wait_prior_check_done( );

  int passed = ppr_check_correctness( );
  if ( task_status == MAIN ) assert( passed );

  signal_check_done( passed );

  if ( !passed )
    BOP_abort_spec( "correctness check failed.");
}

/* Called when spec group is succeeding. */
void _task_group_commit( void ) {
  if (task_status == MAIN) {
    if (bop_mode == SERIAL) {
      task_group_commit( );
      task_group_succ_fini( );
      bop_stats.num_by_main += 1;
      task_status = SEQ;
      return;
    }
    else
      end_clean();//abort( );  /* UNDY has run ahead */
  }

  wait_group_commit_done( );
  task_group_commit( );
  signal_commit_done( );

  bop_msg(2,"The task group (0 to %d) has succeeded", spec_order);

  if (bop_mode == PARALLEL) {
    wait_undy_created( );
    kill( 0, SIGUSR1 );  /* not just get_undy_pid( ) */
    wait_undy_conceded( );
  }

  task_group_succ_fini( );

  bop_msg(5,"Spec wins");

  bop_stats.num_by_spec += spec_order;
  bop_stats.num_by_main += 1;

  task_status = SEQ;
}

/* MAIN-SPEC and SPEC-SPEC commits */
void ppr_task_commit( void ) {
  bop_msg( 4, "ppr task commits" );

  /* Earlier spec aborted further tasks */
  if ( spec_order >= partial_group_get_size( ) ) {
  	  bop_msg( 4, "ppr task outside group size" );
  	  exit(cleanup_children());
      //abort( ); //error
  }

  _ppr_check_correctness( );

  if ( spec_order < partial_group_get_size()-1 ) {
    /* not the last task */
    data_commit( );

    signal_commit_done( );
    if (bop_mode == SERIAL) return;
    wait_next_commit_done( );
    bop_msg( 4, "the next ppr has finished" );
  }

  /* check again in case the next ppr fails after my last check */
  if ( spec_order == partial_group_get_size()-1 ) {
    bop_msg( 3, "task group commits." );
    _task_group_commit( );
    return;
  }

  end_clean();//abort( );
}
void _BOP_group_over(int id){
  if(ppr_static_id != id){
    bop_msg(3, "Mis-matched ppr ids. Continuing");
  }else if(task_status == SPEC){
    bop_msg(3, "Speculative process extended past PPR region. _exiting");
    _exit(0);
  }else{
    bop_msg(3, "Valid state while hitting BOP_group_over. Allowing to pass barrier");
  }
}
void BOP_this_group_over(){
  _BOP_group_over(ppr_static_id);
}
void _BOP_ppr_end(int id) {
  bop_msg(1, "Reached PPR end (pid %d)", getpid());
  if (ppr_pos == GAP || ppr_static_id != id)  {
    bop_msg(4, "Unmatched end PPR (region %d in/after region %d) ignored", id, ppr_static_id);
    return;
  }

  switch (task_status) {
  case SEQ:
    return;
  case MAIN:
    on_main_complete();
    if ( bop_mode != SERIAL && spawn_undy( ) )
      return; /* UNDY */
    /* still MAIN, fall through */
  case SPEC:
    ppr_task_commit( );
    break;
  case UNDY:
    post_ppr_undy( );
    ppr_pos = GAP;
    return;
  default:
    assert(0);
  }
}

/* The arbitration channel for parallel-sequential race between
   speculation processes and understudy.

   There are two needs for "poking" a task when it runs.  The first is
   to abort speculation processes when Undy completes.  The second is
   to signal Undy and wait for its surrender when speculation
   completes.  The first is accomplisned by Undy sending SIGUSR1 to
   SPEC group.  The second is by the succeeding SPEC sending SIGUSR2
   to Undy.

   In addition, the monitor/timing process begins it's clean up process when it recieves SigUsr1.

   When the monitor process recieves SIGUSR2, it immediately kills all running processes using SIGKILL,
   and exits in error.
   All processes have the same SIGUSR1 and SIGUSR2 handlers.

*/
void MonitorInteruptFwd(int signo){
  bop_msg(1, "forwarding signal '%s' to children of pgrp %d", strsignal(signo), monitor_group);
  assert(getpid() == monitor_process_id);
  kill(monitor_group, signo);
  is_monitoring = is_monitoring && signo == SIGINT; //stop monitoring
  if(signo == SIGINT){
    _exit(0); // Don't clean up, just end
  }
}
void print_backtrace(void){
  bop_msg(1, "\nBACKTRACE pid = %d parent pid %d", getpid(), getppid());
#define BUFFER_SIZE 250
  void *bt[BUFFER_SIZE];
  int bt_size;

  bt_size = backtrace(bt, BUFFER_SIZE);
  backtrace_symbols_fd(bt, bt_size, 1);
  bop_msg(1, "\nEND BACKTRACE");
}
void ErrorKillAll(int signo){
  //don't need to reap children. We know that it's an erroring-exit,
  //intecept the call, allert monitor process, execute def behavior
  /**Horrible things are happening. Go to SEQ mode so malloc won't have issues*/
  bop_msg(1, "ERROR CAUGHT %d", signo);
  int om = bop_mode;
  print_backtrace();
  error_alert_monitor();
  signal(signo, SIG_DFL);
  raise(signo);
  bop_mode = om;
}
void SigUsr1(int signo, siginfo_t *siginfo, ucontext_t *cntxt) {
  assert( SIGUSR1 == signo );

  if (task_status == UNDY) {
    bop_msg(3,"Understudy concedes the race (sender pid %d)", siginfo->si_pid);
    signal_undy_conceded( );
    _exit(0); //has no children. don't need to reap
  }
  if (task_status == SPEC || task_status == MAIN) {
    if ( spec_order == partial_group_get_size() - 1 ) return;
    bop_msg(3,"Quiting after the last spec succeeds (sender pid %d)", siginfo->si_pid);
    end_clean(); //wait for children. doesn't return
  }
  if(getpid() == monitor_process_id){
    bop_msg(3, "Monitor process setting is_monitoring to false. Sender %d", siginfo->si_pid);
    is_monitoring = false;
  }
}

/* Used to remove all SPEC tasks and MAIN when UNDY wins, when a spec group
   finishes partially, or BOP_abort_spec_group is called. */
void SigUsr2(int signo, siginfo_t *siginfo, ucontext_t *cntxt) {
  assert( SIGUSR2 == signo );
  assert( cntxt );
  if(getpid() == monitor_process_id){
    bop_msg(1, "Monitor process exiting main loop because of SIGUSR2 (error)", siginfo->si_pid);
    is_monitoring = false;
    errored = true;
  }else if (task_status == SPEC || task_status == MAIN) {
    bop_msg(3,"PID %d exit upon receiving SIGUSR2", getpid());
    _exit(0);
  }
}

void SigBopExit( int signo ){
  bop_msg( 3,"Recieved signal %s (#%d)", strsignal(signo), signo );
  bop_terminal_to_monitor();
  _exit(0); //done. No cleanup, just end the process now
}
/* Initial process heads into this code before forking.
 *
 * Waits until all children have exited before exiting itself.
 */
 #define DEF_EXIT 0 //assume it works unless shown otherwise
int report_child(pid_t child, int status){
  if(child == -1 || !child)
    return DEF_EXIT;
  char * msg;
  int val = -1;
  int rval = DEF_EXIT;
  if(WIFEXITED(status)){
    msg = "Child %d exited with value %d";
    rval = val = WEXITSTATUS(status);
  } else if(WIFSIGNALED(status)) {
    // Have it return true if the signal that kill the process is either SIGABRT or SIGSEGV
    msg = "Child %d was terminated by signal %d";
    val =  WTERMSIG(status);
		if (WTERMSIG(status) == SIGABRT || WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS){
			rval = 1;
      if (child == -monitor_group) {
        bop_msg(1, "The first child died incorrectly! Whole program abort");
        is_monitoring = false;
      }
		}
  }else if(WIFSTOPPED(status)){
    msg = "Child %d was stopped by signal %d";
    val = WSTOPSIG(status);
  }else if(WIFCONTINUED(status)){
    msg = "Child %d was continued";
  }else{
    msg = "Child %d exit unkown status = %d";
    val = status;
  }
  if(val != -1)
    bop_msg(1, msg, child, val);
  else
    bop_msg(1, msg, child);
  return rval;
}
static inline void block_wait(){
  sigset_t set;
  sigemptyset(&set); //block everything
  sigaddset(&set, SIGUSR2);
  sigaddset(&set, SIGUSR1);
  sigprocmask(SIG_BLOCK,&set, NULL);
}
static inline void unblock_wait(){
  //set blocking signals to what it was before block_wait
  sigset_t set;
  sigemptyset(&set); //block everything
  sigaddset(&set, SIGUSR2);
  sigaddset(&set, SIGUSR1);
  sigprocmask(SIG_UNBLOCK, &set, NULL);
}
//don't actually need this
static void wait_process() {
  int status;
  pid_t child;
  assert (monitor_group != 0); //was actually written by the PIPE before this call
  bop_msg(3, "Monitoring pg %d from pid %d (group %d)", monitor_group, getpid(), getpgrp());
  int my_exit = 0; //success
  while (is_monitoring) {
    block_wait();
    if (((child = waitpid(monitor_group, &status, WNOHANG | WUNTRACED)) != -1)) {
      my_exit = my_exit || report_child(child, status); //we only care about zero v. not-zero
    }
    unblock_wait();
  }
  errno = 0;
  //handle remaining processes. Above may not have gotten everything
  block_wait();
  while (((child = waitpid(monitor_group, &status, WUNTRACED)) != -1)) {
    my_exit = my_exit || report_child(child, status); //we only care about zero v. not-zero
  }
  unblock_wait();
  if(errno && errno != ECHILD){
    perror("Error in wait_process. errno != ECHILD. Monitor process endings");
    ipa_teardown();
    _exit(EXIT_FAILURE);
  }
  my_exit = my_exit || errored;
  my_exit = my_exit ? 1 : 0;
  bop_msg(1, "Monitoring process %d ending with exit value %d", getpid(), my_exit);
  msg_destroy();
  kill(monitor_group, SIGKILL); //ensure that everything is killed.
  //Once the monitor process is done, everything should have already terminated_
  if (BOP_get_verbose() >= 3) {
    print_ipa_stats();
  }
   //ipa_teardown();
  _exit(my_exit);
}
int block_signal(int signo){
  sigset_t mask;
  sigemptyset(&mask);
  if(sigaddset(&mask, signo) < 0) {
    return -1;
  }
  sigprocmask(SIG_BLOCK, &mask, NULL);
  return 0;
}
int unblock_signal(int signo){
  sigset_t mask;
  sigemptyset(&mask);
  if(sigaddset(&mask, signo) < 0){
    return -1;
  }
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
  return 0;
}
static void child_handler(int signo){
  assert(signo == SIGCHLD);
  int val = cleanup_children();
  if(val)
    _exit(val);
}
int cleanup_children(){
  int my_exit = 0;
  pid_t child;
  int status;
  while((child = waitpid(monitor_group, &status, WUNTRACED)) != -1){ //any child
    my_exit = my_exit ||  report_child(child, status);
  }
  return my_exit;
}
void end_clean(){
  int exit_code = cleanup_children();
  if(exit_code)
    _exit(exit_code);
  else
    _exit(0);
}

static void BOP_fini(void);

extern int bop_verbose;
extern int errno;
/* Initialize allocation map.  Installs the timer process. */
void __attribute__ ((constructor)) BOP_init(void) {
  //install signal handlers
  /* two user signals for sequential-parallel race arbitration, block
   for SIGUSR2 initially */
  msg_init();
  struct sigaction action;
  sigaction(SIGUSR1, NULL, &action);
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO; //ie only SA_SIGINFO and SA_RESTART
  action.sa_sigaction = (void *) SigUsr1;
  sigaction(SIGUSR1, &action, NULL);
  action.sa_sigaction = (void *) SigUsr2;
  sigaction(SIGUSR2, &action, NULL);

  /** Set up SIGTTOU/SIGTTIN handlers (terminal reads) */
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO | SA_RESTART;
  action.sa_sigaction = (void*) bop_terminal_to_workers;
  sigaction(SIGTTOU, &action, NULL);
  sigaction(SIGTTIN, &action, NULL);


  /* Read environment variables: BOP_GroupSize, BOP_Verbose */
  bop_verbose = get_int_from_env("BOP_Verbose", 0, 6, 0);
  int g = get_int_from_env("BOP_GroupSize", 1, 100, 2);

  BOP_set_group_size( g );
  bop_mode = g<2? SERIAL: PARALLEL;

  /* start the time */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  bop_stats.start_time = tv.tv_sec + (tv.tv_usec/1000000.0);


  /* setting up the timing process and initialize the SEQ task */
  if (bop_mode != SERIAL) {
    /* create a process to allow the use of time command */
    monitor_process_id = getpid();
    block_wait();
    int fd = fork();
    unblock_wait();
    switch (fd) {
    case -1:
      perror("fork() for timer process");
      exit(-1);
    case 0:
      /* Child process continues after switch */
      //We must first set up process group
      //set up a new group

      OWN_GROUP();
      monitor_group = -getpid(); //negative-> work on process group

      break;
    default:
      monitor_group = -fd; //child will set up its monitor_group variable

      bop_msg(1, "Child proc id: %d pgrd %d", getpid(), getpgrp() );
      //forward SIGINT to children/monitor group
      signal( SIGINT, MonitorInteruptFwd ); //sigint gets forwarded to children
      is_monitoring = true; //the real monitor process is the only one to actually loop
      // give the child process the terminal


      wait_process(); //never returns
      _exit(0); /* Should never get here */
    }

    /* the child process continues */

    /* register BOP_End at exit */
    if (atexit(BOP_fini)) {
      perror("Failed to register exit-time BOP_end call");
    }

    /*
    sigset_t mask;
    sigemptyset( &mask );
    sigaddset( &mask, SIGUSR2 );
    sigprocmask( SIG_BLOCK, &mask, NULL );
    */
  }
  assert(getpgrp() == -monitor_group);
  task_status = SEQ;

  /* prepare related signals. Need these???*/
  signal( SIGINT, SigBopExit ); //user-process
  signal( SIGQUIT, SigBopExit );
  signal( SIGTERM, SigBopExit );
  signal( SIGCHLD, child_handler);
  signal( SIGSEGV, ErrorKillAll);

  /* Load ports */
  register_port(&bop_merge_port, "Copy-n-merge Port");
  register_port(&postwait_port, "Post-wait Port");
  register_port(&bop_ordered_port, "Bop Ordered Port");
  register_port(&bop_io_port, "I/O Port");
  register_port(&bop_alloc_port, "Malloc Port");
  bop_msg(3, "Library initialized successfully.");
}
char* status_name(){
  switch (task_status) {
  case SPEC:
    return "SPEC";
  case UNDY:
    return "UNDY";
  case MAIN:
    return "MAIN";
  case SEQ:
    return "SEQ";
  default:
    return "UNKOWN";
  }
}
#include "ipa.h"
static void BOP_fini(void) {
  bop_msg(3, "An exit is reached in %s mode", status_name());
  switch (task_status) {
  case SPEC:
    BOP_abort_spec("SPEC reached an exit");  /* will abort */
    signal(SIGUSR2, SIG_IGN);
    kill(0, SIGUSR2); //send SIGUSR to spec group but not ourself-> own group
    //everything is termininating
    if (bop_mode == SERIAL) {
      data_commit( );
      partial_group_set_size( 1 );
      task_group_commit( );
      task_group_succ_fini( );
    }
    goto kill_monitor;
    break;

  case UNDY:
    kill(0, SIGUSR2);
    kill(-monitor_group, SIGUSR1); //main requires a special signal
    undy_succ_fini( );
    bop_stats.num_by_undy += undy_ppr_count;
    break;

  case MAIN:
  case SEQ:
    break;

  default:
    bop_msg(1, "invalid task status in bop_fini");
    assert(0); //should never get here
  }

  struct timeval tv;
  gettimeofday(&tv, NULL);
  double bop_end_time = tv.tv_sec+(tv.tv_usec/1000000.0);

  bop_msg( 1, "\n***BOP Report***\n The total run time is %.2lf seconds.  There were %d ppr tasks, %d executed speculatively and %d non-speculatively (%d by main and %d by understudy). pid %d\n",
	   bop_end_time - bop_stats.start_time, ppr_index,
	   bop_stats.num_by_spec, bop_stats.num_by_undy + bop_stats.num_by_main, bop_stats.num_by_main, bop_stats.num_by_undy, getpid());

  bop_msg( 3, "A total of %d bytes are copied during the commit and %d bytes are posted during parallel execution. The speculation group size is %d.\n\n",
	   bop_stats.data_copied, bop_stats.data_posted,
	   BOP_get_group_size( ));


  kill_monitor:
    bop_msg(3, "Final process forwarding children...");
    int exitv = cleanup_children();
    bop_msg(3, "Sending shutdown signal to monitor process %d from pid %d", monitor_process_id, getpid());
    kill(monitor_process_id, SIGUSR1);
    bop_msg(3, "Terminal process %d exiting with value %d", getpid(), exitv);
    bop_terminal_to_monitor();
    // dm_print_info();
    if(exitv)
      _exit(exitv);
    //don't need to call normal exit,
}
