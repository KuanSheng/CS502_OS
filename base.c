/************************************************************************

        This code forms the base of the operating system you will
        build.  It has only the barest rudiments of what you will
        eventually construct; yet it contains the interfaces that
        allow test.c and z502.c to be successfully built together.

        Revision History:
        1.0 August 1990
        1.1 December 1990: Portability attempted.
        1.3 July     1992: More Portability enhancements.
                           Add call to sample_code.
        1.4 December 1992: Limit (temporarily) printout in
                           interrupt handler.  More portability.
        2.0 January  2000: A number of small changes.
        2.1 May      2001: Bug fixes and clear STAT_VECTOR
        2.2 July     2002: Make code appropriate for undergrads.
                           Default program start is in test0.
        3.0 August   2004: Modified to support memory mapped IO
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking
	4.0  July    2013: Major portions rewritten to support multiple threads
************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <stdio.h>
#include             <stdlib.h>

// These loacations are global and define information about the page table
extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;
extern long Z502_REG1;
extern long Z502_REG2;
extern long Z502_REG3;
extern long Z502_REG4;
extern long Z502_REG5;
extern long Z502_REG6;
extern long Z502_REG7;
extern long Z502_REG8;
extern long Z502_REG9;
#define READY 1
#define TIMER 2
#define CURRENT_RUNNING 3
int         Pid = 0;
int         NextInterruptTime=0;
int                 Time;
int                 Temp;
int                 Toppriority;
int                 maxbuffer = 0;
int                 actual_length;
int                 msgnum;
INT32 LockResult;
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
extern void          *TO_VECTOR [];

char                 *call_names[] = { "mem_read ", "mem_write",
                            "read_mod ", "get_time ", "sleep    ",
                            "get_pid  ", "create   ", "term_proc",
                            "suspend  ", "resume   ", "ch_prior ",
                            "send     ", "receive  ", "disk_read",
                            "disk_wrt ", "def_sh_ar" };


/************************************************************************
    INTERRUPT_HANDLER
        When the Z502 gets a hardware interrupt, it transfers control to
        this routine in the OS.
************************************************************************/
//define process;
typedef struct PCB{
        char Processname[20];
        int status;
        long Priority;
		int pid;
        int reg1;
        int reg2;
		int wakeuptime;
        void *context;
        struct PCB *next;
        }PCB;

typedef struct message{
	   long    target_pid;
	   long    source_pid;
	   long    msg_length;
	   char  msg_buffer[30];
	   struct message *next;
	   
}message; 
typedef struct Frame{
	  UINT16 pagenumber;
	  UINT16 framenumber;
	  UINT16 pid;
	  UINT16 framestatus;//free or not free;
	  struct Frame *next;
}Frame;

 PCB *readyfront = NULL;
 PCB *readyrear = NULL;
 PCB *timerfront = NULL;
 PCB *timerrear = NULL;
 PCB *diskfront = NULL;
 PCB *diskrear = NULL;
 PCB *suspendfront;
 PCB *suspendrear;
 PCB *Running;
 PCB *NextRunning;
 PCB *tempPCB;

 message *msgfront;
 message *msgrear;
 message *checkmsg;
 Frame *framefront;
 Frame *framerear;

 int checkPCBname(PCB *pcbc){
	       int i = 1;
	       PCB *head = readyfront;
           
           
           while(head != NULL){
                if(strcmp(pcbc->Processname, head->Processname) == 0){
                  i = 0;
                  break;
                  }
                else
                  head = head->next;
                               
                            }

		          //free(head);
                  return(i);
}
int checkReady(int id){
	 int flag = 0;
	 PCB *head = readyfront;
	 while(head!=NULL){
		 if(head->pid == id){
			 flag = 1;
			 return(flag);
		 }
		 else
			 head =head->next;
	 }

	 return(flag);
 }
int checkTimer(int id){
	 int flag = 0;
	 PCB *head = timerfront;
	 while(head!=NULL){
		 if(head->pid == id){
			 flag = 1;
			 return(flag);
		 }
		 else
			 head =head->next;
	 }

	 return(flag);
 }

int checkSuspend(int id){
	int flag = 0;
	 PCB *head = suspendfront;
	 while(head!=NULL){
		 if(head->pid == id){
			 flag = 1;
			 return(flag);
		 }
		 else
			 head =head->next;
	 }

	 return(flag);

 }

 void os_out_ready(PCB *current){
	 PCB *head = readyfront;
	 if(current == readyfront){
		 if(current->next == NULL){
			 readyfront = NULL;
			 readyrear = NULL;
		 }
		 else{
			 readyfront = current->next;
			 current->next=NULL;
			 Toppriority = readyfront->Priority;
		 }
			 
	 }
	 else{
		 if(current->next == NULL){
			 while(head->next!=current)
				 head=head->next;
			 head->next=NULL;
		 }
		 else{
			 while(head->next!=current)
				 head=head->next;
			 head->next=current->next;
			 current->next = NULL;
		 }
	 }
	 Running = current;
	 NextRunning = readyfront;

	 
}

 void add_to_timerQ(){
	 PCB *head = timerfront;
	 PCB *head1 = timerfront;
	 
	if(timerfront == NULL&&timerrear == NULL){
		timerfront = Running;
		timerrear = Running;
		
		NextInterruptTime = Running->wakeuptime;
	}
	else{
		
		if(Running->wakeuptime < NextInterruptTime){
			Running->next = timerfront;
		    timerfront = Running;
			
			NextInterruptTime = Running->wakeuptime;
		}
		else{
			while(head->next!=NULL){
				if(Running->wakeuptime < head->wakeuptime)
					break;
				head=head->next;
			}
			if(head->next!=NULL){
				while(head1->next!=head)
					head1=head1->next;
				Running->next = head;
				head1->next=Running;
				
				
			}
			else{
				if(Running->wakeuptime < head->wakeuptime){
					while(head1->next!=head)
						head1=head1->next;
				   Running->next = head;
				   head1->next=Running;
				   
				}
				else{
					head->next = Running;
					Running->next=NULL;
					timerrear = Running;
					
				}
			}
		}

	}

	Running=NULL;
	
 }

 void return_readyQ(PCB *pcb){
     PCB *head = readyfront;
	 PCB *head1 = readyfront;

	 if(readyfront==NULL&&readyrear==NULL){
		 readyfront = pcb;
		 readyrear = pcb;
		 Toppriority = pcb->Priority;
		 pcb->wakeuptime = 0;
		 pcb->next=NULL;
	 }
	 else{
		 if(pcb->Priority <= Toppriority){
			 pcb->next = readyfront;
			 readyfront = pcb;
			 Toppriority = pcb->Priority;
		 }
		 else{
			 while(head->next!=NULL){
				 if(pcb->Priority < head->Priority)
				 {break;}
				 head = head->next;}
				 
				 if(head->next!=NULL){
					 while(head1->next!=head){
					   head1=head1->next;
					 }
					head1->next=pcb;
					pcb->next=head;
				 }
				 else{
					 if(pcb->Priority < head->Priority ){
						 while(head1->next!=head){
							 head1=head1->next;
						 }
						 head1->next=pcb;
						 pcb->next = head;
					 }
					 else{
						 head->next=pcb;
						 readyrear=pcb;
					 }
				 }
			 }
		 }
 }

 void out_of_timerQ( PCB *pcb ){
	 int currenttime;
	 PCB *head = timerfront;
	 if(pcb!=timerfront){
		 if(pcb->next == NULL){
			while(head->next!=pcb)
				head= head->next;
			head->next=NULL;
			pcb->next = NULL;
			return_readyQ(pcb);
		 }
		 else{
			 while(head->next!=pcb)
				 head= head->next;
			 head->next = pcb->next;
			 pcb->next = NULL;
			 return_readyQ(pcb);
		 }
	 }
	 else{
		 if(pcb->next == NULL){
			 timerfront = NULL;
			 timerrear = NULL;
			 pcb->next = NULL;
			 NextInterruptTime = 0;
			 return_readyQ(pcb);
		 }
		 else{
			 timerfront = pcb->next;
			 NextInterruptTime = pcb->next->wakeuptime;
			 pcb->next = NULL;
			 return_readyQ(pcb);
			 //MEM_READ(Z502ClockStatus, &currenttime);
			 //if(currenttime > NextInterruptTime)
				 //out_of_timerQ(pcb->next);

		 }
	 }
 }
 void change_priority_ready(PCB *pcb,int new_priority){
	 PCB *head = readyfront;
	 PCB *head1 = readyfront;

	 if(pcb == readyfront){
		 if(pcb->next == NULL){
			 readyfront = NULL;
			 readyrear = NULL;
		 }
		 else{
			 readyfront = pcb->next;
			 pcb->next=NULL;
			 Toppriority = readyfront->Priority;
		 }
			 
	 }
	 else{
		 if(pcb->next == NULL){
			 while(head->next!=pcb)
				 head=head->next;
			 head->next=NULL;
		 }
		 else{
			 while(head->next!=pcb)
				 head=head->next;
			 head->next=pcb->next;
			 pcb->next = NULL;
		 }
	 }

	  pcb->Priority = new_priority;
	  head = readyfront;

	  return_readyQ(pcb);
 }
 void change_priority_timer(PCB *pcb,int new_priority){
	 pcb->Priority = new_priority;
 }
 void change_priority_running(int new_priority){
	 Running->Priority = new_priority;
 }
 void change_priority_suspend(PCB *pcb,int new_priority){
	 pcb->Priority = new_priority;
 }
 void resume_process(PCB *pcb){
	 int         i;
	 PCB *head = suspendfront;
	 if(pcb == suspendfront){
		 suspendfront=NULL;
		 suspendrear=NULL;
		 pcb->next=NULL;
	 }
	 else{
		 while(head->next!=pcb){
			 head=head->next;
		 }
		 if(pcb->next==NULL){
			 head->next=NULL;
			}
		 else{
			 head->next=pcb->next;
			 pcb->next=NULL;
		 }
	 }
	  return_readyQ(pcb);	
	  Z502_REG9=ERR_SUCCESS;

 }
 void suspend_process_ready(PCB *pcb){

	 PCB *head = readyfront;
	 if(pcb == readyfront){
		 if(pcb->next == NULL){
			 readyfront = NULL;
			 readyrear = NULL;
		 }
		 else{
			 readyfront = pcb->next;
			 pcb->next=NULL;
			 Toppriority = readyfront->Priority;
		 }
			 
	 }
	 else{
		 if(pcb->next == NULL){
			 while(head->next!=pcb)
				 head=head->next;
			 head->next=NULL;
		 }
		 else{
			 while(head->next!=pcb)
				 head=head->next;
			 head->next=pcb->next;
			 pcb->next = NULL;
		 }
	 }

	 if(suspendfront==NULL&&suspendrear==NULL){
		 suspendfront = pcb;
		 suspendrear = pcb;
		 pcb->next = NULL;
	 }
	 else{
		 suspendrear->next = pcb;
		 suspendrear = pcb;
		 pcb->next = NULL;
	 }
	
 }

void suspend_process_timer( PCB *pcb )
{
	 PCB *head = timerfront;
	 if(pcb!=timerfront){
		 if(pcb->next == NULL){
			while(head->next!=pcb)
				head= head->next;
			head->next=NULL;
			pcb->next = NULL;
		 }
		 else{
			 while(head->next!=pcb)
				 head= head->next;
			 head->next = pcb->next;
			 pcb->next = NULL;
		 }
	 }
	 else{
		 if(pcb->next == NULL){
			 timerfront = NULL;
			 timerrear = NULL;
			 pcb->next = NULL;
			 NextInterruptTime = 0;
		 }
		 else{
			 timerfront = pcb->next;
			 NextInterruptTime = pcb->next->wakeuptime;
			 pcb->next = NULL;
		 }
	 	 
	 if(suspendfront==NULL&&suspendrear==NULL){
		 suspendfront = pcb;
		 suspendrear = pcb;
		 pcb->next = NULL;
	 }
	 else{
		 suspendrear->next = pcb;
		 suspendrear = pcb;
		 pcb->next = NULL;
	 }
}
}

void os_delete_process_ready(int process_id){
	     int i = 0;
	     PCB *head = readyfront;
		 PCB *head1 = readyfront;

		 while(head!=NULL){
			 if(head->pid == process_id){
				 i = 1;
				 break;
			 }
			 head = head->next;
		 }

		 if(i=1&&head == readyfront){
			 if(head->next == NULL)
				{ readyfront = NULL;
			     readyrear = NULL;
				 free(head);
				 Pid--;
			 }
			 else {
				 readyfront = head->next;
				 free(head);
				 Pid--;
			 }
		 }
		 else if(i=1&&head!= readyfront){
			 if(head1->next == NULL){
				 while(head1->next != head){
					 head1= head1->next;
				 }
				 readyrear = head1;
				 readyrear->next = NULL;
				 free(head);
				 Pid--;
			 }
			 else{
			    while(head->next != head)
					head1 = head1-> next;
				head1->next=head->next;
				free(head);
				Pid--;
			 }
		 }
		 else 
			 printf("can not find the process %d",&process_id);
			 

		
 }
void send_message(int sid,int tid,int len,char* msgbuff){
	message *msg=(message *)malloc(sizeof(message));
	msg->source_pid = sid;
	msg->target_pid = tid;
	msg->msg_length = len;
	strcpy(msg->msg_buffer,msgbuff);
	
	if(msgfront == NULL&&msgrear == NULL){
		msgfront = msg;
		msgrear = msg;
		msg->next = NULL;
	}
	else{
		msgrear->next = msg;
		msgrear = msg;
		msg->next=NULL;
	}

}
void send_message_to_all(int sid,int len,char* msgbuff){
	message *msg=(message *)malloc(sizeof(message));
	msg->source_pid = sid;
	msg->target_pid = 99;
	msg->msg_length = len;
	strcpy(msg->msg_buffer,msgbuff);
	
	if(msgfront == NULL&&msgrear == NULL){
		msgfront = msg;
		msgrear = msg;
		msg->next = NULL;
	}
	else{
		msgrear->next = msg;
		msgrear = msg;
		msg->next = NULL;
	}

}
void msg_out_queue(message *msg){
	message *head = msgfront;

	if(msg == msgfront){
		if(msg->next == NULL){
			msgfront = NULL;
			msgrear = NULL;
		}
		else{
			msgfront = msg->next;
			msg->next = NULL;
		}
	}
	else{
		while(head->next == msg){
			head = head->next;
		}
		if(msg->next!=NULL){
			head->next = msg->next;
			msg->next = NULL;
		}
		else{
			head->next = NULL;
		}
	}
}
void receive_message_fromall(){
	int flag = 0;
	int i = 0;
	
	message *head = msgfront;

	while(head!=NULL){
		if(head->target_pid == Running->pid||head->target_pid == 99){
			
	        {flag =1;
			break;}

		}
			else head = head->next;
	}
	
	if(flag == 1){
	checkmsg = head;
	msgnum++;
	//	//Z502_REG9 = ERR_SUCCESS;
	}

} 		
void receive_message_fromone(int id){
	int flag = 0;
	int i = 0;
	
	message *head = msgfront;

	while(head!=NULL){
		if(head->target_pid == Running->pid||head->target_pid == Running->pid == 99){
			if(head->source_pid = id){
	       
			
			//msg_out_queue(head);
			flag = 1;
			break;
			}
		}
			else head = head->next;
	}

	if(flag == 1){
	checkmsg = head;
	msgnum++;
	//	//Z502_REG9 = ERR_SUCCESS;
	}
	
}
 
 int os_get_process_id(char *name){
	 int i = 0;
	 int j = 0;
	 int m = 0;
	 PCB *head = readyfront;
	 PCB *head1 = timerfront;
	 if(name == ""){
		     Z502_REG9 = ERR_SUCCESS;
			 i=Running->pid;
			 return(i);
	    }
	 else while(head!=0){
		 if(strcmp(head->Processname,name) == 0){
			 i = 1;
			 break;
		 }
		 else 
			 head = head->next;
		  }

	 if(i == 1){
		 return( head->pid );
		 Z502_REG9 = ERR_SUCCESS;
	 }
	 else 
	 {//printf("can not find the process");
	 //Z502_REG9++;
		 j=1;
	 }

	if(j == 1 ){
	 while(head1!=0){
	 if(strcmp(head1->Processname,name) == 0){
		 i = 1;
		 break;
		 }
		 else 
		 head1 = head1->next;
		  }

	 if(i == 1){
		 return( head1->pid );
		 Z502_REG9 = ERR_SUCCESS;
	 }
	 else 
	 {m=1;}
	 }

	 if(m==1&&j==1){printf("can not find the process!");
	 Z502_REG9++;}
 }
//create process;    
    
void    os_create_process( char* process_name, void* scontext, long Prio ) {
        void                *next_context;
        PCB *pcb = (PCB *)malloc(sizeof(PCB));
        strcpy(pcb->Processname , process_name);
        pcb->status = 0;
        pcb->Priority = Prio;
        pcb->context = scontext;
		pcb->pid = Pid;
		//readyfront = pcb;
		//readyrear = pcb;
		pcb->next=NULL;
		pcb->status=CURRENT_RUNNING;
		Running = pcb;
		Pid++;
        Z502MakeContext( &next_context, pcb->context, USER_MODE );
        Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &next_context );
}
void set_printer(){

}
void dispatcher(){
	while(readyfront==NULL&&readyrear==NULL)
	{CALL(Z502Idle());}
	
		os_out_ready(readyfront);

}
void start_timer( int delaytime ){
        
        int                 Status;
		PCB                 *head;
		Temp = delaytime;
        //MEM_READ( Z502TimerStatus, &Status);
		//READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
		if(Running == NULL){
			dispatcher();
			Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &(Running->context) );
		}
		
		MEM_READ(Z502ClockStatus, &Time);
		MEM_WRITE( Z502TimerStart, &Temp);
        Running->wakeuptime = Time+Temp;
		add_to_timerQ();
	    dispatcher();
		printf("\n");
		printf("cunrrent running pid %d\n",Running->pid);
		printf("\n");

		head = readyfront;
		printf("readyqueue:");
		while(head!=NULL){
			
			printf("%d(%d)\t",head->pid,head->Priority);
			head=head->next;
		}
		printf("\n");
		printf("\n");
		head = timerfront;
		printf("timerqueue:");
		while(head!=NULL){
			
			printf("%d(%d)\t",head->pid,head->Priority);
			
			head=head->next;
		}
		printf("\n");
		printf("\n");
		head=suspendfront;
		printf("suspendqueue:");
			while(head!=NULL){
				printf("%d(%d)\t",head->pid,head->Priority);
				head=head->next;

			}
		printf("\n");
		printf("\n");
	   //READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
	   if(Running != NULL){
		   Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &(Running->context) );
	   }
	   else
	   {CALL(Z502Idle());}
		//Z502Idle();

	 
}
Frame* get_free_frame(){
	Frame *head = framefront;

	while(head != NULL){
		if(head->framestatus == 0)
			return head;

		else 
			head = head->next;
	}

	return NULL;
}
void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;
    static BOOL        remove_this_in_your_code = TRUE;   /** TEMP **/
    static INT32       how_many_interrupt_entries = 0;    /** TEMP **/
	int                current_time;
	int                newtime;
	
    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

    /** REMOVE THE NEXT SIX LINES **/
	//tempPCB = timerfront;
	while(timerfront!=NULL){
	MEM_READ(Z502ClockStatus, &current_time);
    if(current_time > NextInterruptTime)
	{out_of_timerQ(timerfront);}
	 else break;
	}

	if(timerfront!=NULL){
		MEM_READ(Z502ClockStatus, &current_time);
		newtime = timerfront->wakeuptime - current_time;
		MEM_WRITE( Z502TimerStart, &newtime);
	}
	
	 //return_readyQ(tempPCB);
}                                       /* End of interrupt_handler */
/************************************************************************
    FAULT_HANDLER
        The beginning of the OS502.  Used to receive hardware faults.
************************************************************************/

void    fault_handler( void )
    {
    INT32       device_id;
    INT32       status;
    INT32       Index = 0;
	Frame       *f;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

    printf( "Fault_handler: Found vector type %d with value %d\n",
                        device_id, status );
	if(status>=1024){
		printf("page overflow!\n");
		Z502Halt();
	}
	if(Z502_PAGE_TBL_ADDR == NULL){
	Z502_PAGE_TBL_LENGTH = 1024;
	Z502_PAGE_TBL_ADDR = (UINT16 *) calloc(sizeof(UINT16),
		Z502_PAGE_TBL_LENGTH);}
	//here we initialize the page table
	f = get_free_frame();
	if(f != NULL){
		f->pid = Running->pid;
		f->pagenumber = status;
		f->framenumber = 1;
		Z502_PAGE_TBL_ADDR[f->pagenumber] = (UINT16)f->framenumber | 0x8000;
		
	}


	if(device_id==4&&status==0){
		Z502Halt();
	}
    // Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );
}                                       /* End of fault_handler */

/************************************************************************
    SVC
        The beginning of the OS502.  Used to receive software interrupts.
        All system calls come to this point in the code and are to be
        handled by the student written code here.
        The variable do_print is designed to print out the data for the
        incoming calls, but does so only for the first ten calls.  This
        allows the user to see what's happening, but doesn't overwhelm
        with the amount of data.
************************************************************************/

void    svc( SYSTEM_CALL_DATA *SystemCallData ) {
    short               call_type;
    static short        do_print = 10;
    short               i;
    int                 Time;
	int                 ID;
	int                 k=0,m=0,n=0;
	int                 PR;
	int                 sid,tid,mlen;
	int                 actual_length;
    long                tmp;
	INT32               diskstatus;
	long                tmpid;
	void                *next_context;
	char                *tmpmsg;
	PCB                 *head;
	PCB                 *head1;
    PCB *pcb = (PCB *)malloc(sizeof(PCB));
    

    call_type = (short)SystemCallData->SystemCallNumber;
    if ( do_print > 0 ) {
        printf( "SVC handler: %s\n", call_names[call_type]);
        for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++ ){
        	 //Value = (long)*SystemCallData->Argument[i];
             printf( "Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
             (unsigned long )SystemCallData->Argument[i],
             (unsigned long )SystemCallData->Argument[i]);
		}}
        switch(call_type){
        case SYSNUM_GET_TIME_OF_DAY:
               MEM_READ(Z502ClockStatus, &Time);
			   //Z502_REG1=Time;
               *SystemCallData->Argument[0]=Time;
               break;
        
        case SYSNUM_TERMINATE_PROCESS:
              tmpid = (long)SystemCallData->Argument[0];
			  if(tmpid>=0){
			  os_delete_process_ready(tmpid);
			  Z502_REG9 = ERR_SUCCESS;}
			  else if(tmpid == -1)
			    {   
					head =Running;
			    Running = NULL;
				while(readyfront==NULL&&timerfront==NULL){
					Z502Halt();
				}
				//free(head);
			  dispatcher();
			  Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &(Running->context) );
			  }
			  else
				    Z502Halt();
			  break;
              //the execution of sleep();
        case SYSNUM_SLEEP:
             start_timer( (int)SystemCallData->Argument[0] );

             break;
		case SYSNUM_GET_PROCESS_ID:
			*SystemCallData->Argument[1] = os_get_process_id((char *)SystemCallData->Argument[0]); 
			break;
        case SYSNUM_CREATE_PROCESS:
             strcpy(pcb->Processname , (char *)SystemCallData->Argument[0]);
             pcb->Priority = (long)SystemCallData->Argument[2];
			 head = readyfront;
			 head1 = readyfront;
             if(Pid < 20){
             if(pcb->Priority >0){
		       if(readyfront == NULL&&readyrear == NULL){
                 readyfront = pcb;
                 readyrear = pcb;
                 //*SystemCallData->Argument[4] = ERR_SUCCESS;
                 Z502_REG9 = ERR_SUCCESS;
				 pcb->pid = Pid;
				 pcb->next=NULL;
				 Toppriority = pcb->Priority;
				 *SystemCallData->Argument[3] = Pid;
				 //pcb->context = (void *)SystemCallData->Argument[1];
				Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE );
				pcb->context = next_context;
				 Pid++;
                 }
               else if(readyfront!=NULL&&readyrear!=NULL){

				 if(checkPCBname(pcb) == 1){

				  if(pcb->Priority < Toppriority){
                    Z502_REG9 = ERR_SUCCESS;
				    pcb->next = readyfront;
				    readyfront = pcb;
				    pcb->pid = Pid;
				    pcb->next=NULL;
				    Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE );
				    pcb->context = next_context;
                    *SystemCallData->Argument[3] = Pid;
			        Pid++;
				  }
				  else{
					  while(head->next!=NULL){
						  if(pcb->Priority < head->Priority)
							  break;
						  else
						  head = head->next;}

					  if(head->next!=NULL){
						  while(head1->next!=head)
						  {head1=head1->next;}
						  Z502_REG9 = ERR_SUCCESS;
						  head1->next = pcb;
						  pcb->next=head;
						  pcb->pid = Pid;
						  Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE );
				          pcb->context = next_context;
                          *SystemCallData->Argument[3] = Pid;
			              Pid++;
					  }
					  else{
						  if(pcb->Priority < head->Priority){
							  while(head1->next!=head)
							  {head1=head1->next;}
							  Z502_REG9 = ERR_SUCCESS;
							  head1->next=pcb;
							  pcb->next=head;
							  pcb->pid = Pid;
						      Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE );
				              pcb->context = next_context;
                              *SystemCallData->Argument[3] = Pid;
			                  Pid++;
						  }
						  else{
							  Z502_REG9 = ERR_SUCCESS;
							  head->next = pcb;
							  readyrear = pcb;
							  pcb->next=NULL;
							  pcb->pid = Pid;
						      Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE );
				              pcb->context = next_context;
                              *SystemCallData->Argument[3] = Pid;
			                  Pid++;
						  }
					  }
					   
				  }
				 }
				 else free(pcb);
                  }
			 }else free(pcb);
			 }
             else{
				 Z502_REG9++;
				 free(pcb);}
        break;

		case SYSNUM_SUSPEND_PROCESS:
			ID = (int)SystemCallData->Argument[0];
			head = suspendfront;
			while(head!=NULL){
				if(head->pid == ID){
					n = 1;
					break;
				}
				else
					head = head->next;
			}

			if(n!=1){
			head = readyfront;
			while(head!=NULL){
				if(head->pid == ID){
					Z502_REG9 = ERR_SUCCESS;
					suspend_process_ready(head);
					k = 1;
					break;
				}
				else
					head = head->next;
           }

			if(k == 0){
				head = timerfront;
				while(head!=NULL){
					if(head->pid == ID){
						Z502_REG9 = ERR_SUCCESS;
						suspend_process_timer(head);
					    m = 1;
					    break;
					}
					else
						head=head->next;
				}
				if(m == 0&&k == 0){
					printf("illegal PID\n");
				}
			}
		}
			if(n == 1){
				printf("can not suspend suspended process\n");
			}
		break;

		case SYSNUM_RESUME_PROCESS:
			ID = (int)SystemCallData->Argument[0];
			head = suspendfront;
			while(head!=NULL){
				if(head->pid == ID){
					k=1;
					break;}
				else 
					head=head->next;
			}
			if(k==1)
			resume_process(head);
			else 
			printf("error\n");
		break;
		case SYSNUM_CHANGE_PRIORITY:
			ID = (int)SystemCallData->Argument[0];
			PR = (int)SystemCallData->Argument[1];
			if(ID == -1){
				Running->Priority = PR;
				Z502_REG9 = ERR_SUCCESS;
			}
			else{
			if(PR < 100){
			if(checkReady(ID) == 1){
				head = readyfront;
				while(head->pid != ID)
					head=head->next;
				change_priority_ready(head,PR);
				Z502_REG9 = ERR_SUCCESS;
			}
			else if(checkTimer(ID) == 1){
				head = timerfront;
				while(head->pid != ID)
					head=head->next;
				change_priority_timer(head,PR);
				Z502_REG9 = ERR_SUCCESS;
			}
			else if(checkSuspend(ID) == 1){
				head = suspendfront;
				while(head->pid != ID)
					head = head->next;
				change_priority_suspend(head,PR);
				Z502_REG9 = ERR_SUCCESS;
			}
			else{
				printf("ID ERROR!\n");
			}
			}
			else{
				printf("illegal Priority");
			}
			}
		break;
		case SYSNUM_SEND_MESSAGE:
			sid = Running->pid;
			tid = (int)SystemCallData->Argument[0];
			tmpmsg =(char *)SystemCallData->Argument[1];
			mlen = (int)SystemCallData->Argument[2];
			if(maxbuffer < 8){
			if(tid < 100){
				if(mlen < 100)
				{
					if(tid>0)
					{send_message(sid,tid,mlen,tmpmsg);
					maxbuffer++;}
					else if(tid == -1){
						send_message_to_all(sid,mlen,tmpmsg);
						maxbuffer++;
					}
				}
				else{
					printf("illegal length!\n");
				}
			} else
				printf("illegal id!\n");
			}
			else 
			{printf("no space!\n");
			Z502_REG9++;
			}
		break;
		case  SYSNUM_RECEIVE_MESSAGE:
			sid = (int)SystemCallData->Argument[0];
			mlen = (int)SystemCallData->Argument[2];

			if(sid < 100){
				if(mlen < 100){
					if(sid == -1){

					receive_message_fromall();
					if(msgnum>0){
					actual_length = strlen(checkmsg->msg_buffer);
					if(mlen >actual_length){
						msg_out_queue(checkmsg);
						*SystemCallData->Argument[3] = actual_length;
						*SystemCallData->Argument[4] = checkmsg->source_pid;
						strcpy((char *)SystemCallData->Argument[1] ,checkmsg->msg_buffer);
						Z502_REG9 = ERR_SUCCESS;
					}
					else{
						printf("small buffer!\n");
					}
					}
			}
					else{
					receive_message_fromone(sid);
					if(msgnum>0){
					actual_length = strlen(checkmsg->msg_buffer);
					if(mlen >actual_length){
						msg_out_queue(checkmsg);
						*SystemCallData->Argument[3] = actual_length;
						*SystemCallData->Argument[4] = checkmsg->source_pid;
						strcpy((char *)SystemCallData->Argument[1], checkmsg->msg_buffer);
						Z502_REG9 = ERR_SUCCESS;
					}
					else{
						printf("small buffer!\n");
					}
					}
					} 
				}
				else
					printf("illegal length!\n");
			}
			else
				printf("illegal id!\n");
		break;
		case  SYSNUM_DISK_READ:
			MEM_WRITE(Z502DiskSetID, &SystemCallData->Argument[0]);
                        MEM_READ(Z502DiskStatus, &diskstatus);
                        if (diskstatus == DEVICE_FREE)        // Disk hasn't been used - should be free
                                printf("Got expected result for Disk Status\n");
                        else
                                printf("Got erroneous result for Disk Status - Device not free.\n");
                        MEM_WRITE(Z502DiskSetSector, &SystemCallData->Argument[1]);
                        MEM_WRITE(Z502DiskSetBuffer, (INT32*)SystemCallData->Argument[2]);
                        diskstatus = 0;                        // Specify a read
                        MEM_WRITE(Z502DiskSetAction, &diskstatus);
                        diskstatus = 0;                        // Must be set to 0
                        MEM_WRITE(Z502DiskStart, &diskstatus);
                        MEM_WRITE(Z502DiskSetID, &SystemCallData->Argument[0]);
                        MEM_READ(Z502DiskStatus, &diskstatus);
                        while (diskstatus != DEVICE_FREE) 
                        {
                                Z502Idle();
                                MEM_READ(Z502DiskStatus, &diskstatus);
                        }
                        break;
		break;
		case SYSNUM_DISK_WRITE:
		/* Do the hardware call to put data on disk */
	     MEM_WRITE(Z502DiskSetID, &SystemCallData->Argument[0]);
	     MEM_READ(Z502DiskStatus, &diskstatus);
	     if (diskstatus == DEVICE_FREE)        // Disk hasn't been used - should be free
		   printf("Got expected result for Disk Status\n");
	     else
		   printf("Got erroneous result for Disk Status - Device not free.\n");
	     MEM_WRITE(Z502DiskSetSector, &SystemCallData->Argument[1]);
	     MEM_WRITE(Z502DiskSetBuffer, (INT32 * )SystemCallData->Argument[2]);
	      diskstatus = 1;                        // Specify a write
	     MEM_WRITE(Z502DiskSetAction, &diskstatus);
	       diskstatus = 0;                        // Must be set to 0
	     MEM_WRITE(Z502DiskStart, &diskstatus);
	// Disk should now be started - let's see
	     MEM_WRITE(Z502DiskSetID, &SystemCallData->Argument[0]);
	     MEM_READ(Z502DiskStatus, &diskstatus);
	     while (diskstatus != DEVICE_FREE) {
		   Z502Idle();
		   MEM_READ(Z502DiskStatus, &diskstatus);
	     }
		break;
        default: printf("call_type %d cannot be recognized\n",call_type);
        break;
        }

    do_print--;
}
                                          // End of svc



/************************************************************************
    osInit
        This is the first routine called after the simulation begins.  This
        is equivalent to boot code.  All the initial OS components can be
        defined and initialized here.
************************************************************************/

void    osInit( int argc, char *argv[]  ) {
    void                *next_context;
    INT32               i;
    void                *scontext = (void *)test1a;
    int                 Prio = 8; 
	int                 j,frameindex = 0;
	PCB *pcb = (PCB *)malloc(sizeof(PCB));
	char name[20] ="testbase";
	char test[20];
	Frame *frame;

	/*let's just initialize all the frames here*/
	for(j=0;j<64;j++){
		frame = (Frame *)malloc(sizeof(Frame));
		frame->framenumber = frameindex;
		frame->framestatus = 0;
		frame->next = NULL;

		if(framefront == NULL){
			framefront = frame;
			framerear = frame;
		}

		else{
			framerear->next = frame;
			framerear = frame;
		}
		frameindex++;
	}

    /* Demonstrates how calling arguments are passed thru to here       */

    printf( "Program called with %d arguments:", argc );
    for ( i = 0; i < argc; i++ )
        printf( " %s", argv[i] );
    printf( "\n" );
    printf( "Calling with argument 'sample' executes the sample program.\n" );

    /*          Setup so handlers will come to code in base.c           */

    TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR]   = (void *)interrupt_handler;
    TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)fault_handler;
    TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR]  = (void *)svc;

    /*  Determine if the switch was set, and if so go to demo routine.  */

    if (( argc > 1 ) && ( strcmp( argv[1], "sample" ) == 0 ) ) {
        Z502MakeContext( &next_context, (void *)sample_code, KERNEL_MODE );
        Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &next_context );
    }                   /* This routine should never return!!           */
     //os_create_process("testbase", scontext, 10);
    /*  This should be done by a "os_make_process" routine, so that
        test0 runs on a process recognized by the operating system.    */
	 strcpy(pcb->Processname , name);
        pcb->status = 0;
        pcb->Priority = Prio;
       
		pcb->pid = Pid;
		//readyfront = pcb;
		//readyrear = pcb;
		pcb->next=NULL;
		//pcb->status=CURRENT_RUNNING;
		Running = pcb;
		Pid++;

		printf("please input the test you want to enter:\n");
		scanf("%s",&test);
		if(strcmp(test,"test1a")==0){
			Z502MakeContext( &next_context, (void *)test1a, USER_MODE );
		}
		else if(strcmp(test,"test1b")==0){
			Z502MakeContext( &next_context, (void *)test1b, USER_MODE );
		}
		else if(strcmp(test,"test1c")==0){
			Z502MakeContext( &next_context, (void *)test1c, USER_MODE );
		}
		else if(strcmp(test,"test1d")==0){
			Z502MakeContext( &next_context, (void *)test1d, USER_MODE );
		}
		else if(strcmp(test,"test1e")==0){
			Z502MakeContext( &next_context, (void *)test1e, USER_MODE );
		}
		else if(strcmp(test,"test1f")==0){
			Z502MakeContext( &next_context, (void *)test1f, USER_MODE );
		}
		else if(strcmp(test,"test1g")==0){
			Z502MakeContext( &next_context, (void *)test1g, USER_MODE );
		}
		else if(strcmp(test,"test1h")==0){
			Z502MakeContext( &next_context, (void *)test1h, USER_MODE );
		}
		else if(strcmp(test,"test1i")==0){
			Z502MakeContext( &next_context, (void *)test1i, USER_MODE );
		}
		else if(strcmp(test,"test1j")==0){
			Z502MakeContext( &next_context, (void *)test1j, USER_MODE );
		}
		else if(strcmp(test,"test1k")==0){
			Z502MakeContext( &next_context, (void *)test1k, USER_MODE );
		}
		else if(strcmp(test,"test2a")==0){
			Z502MakeContext( &next_context, (void *)test2a, USER_MODE );
		}
		else if(strcmp(test,"test2b")==0){
			Z502MakeContext( &next_context, (void *)test2b, USER_MODE );
		}
		else if(strcmp(test,"test2c")==0){
			Z502MakeContext( &next_context, (void *)test2c, USER_MODE );
		}

    //Z502MakeContext( &next_context, (void *)test1e, USER_MODE );
	pcb->context = next_context;
    Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &next_context );
}                                               // End of osInit
