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

typedef struct Queue{
	   PCB *node;
	   struct Queue *next;
}Queue;       

 PCB *readyfront = NULL;
 PCB *readyrear = NULL;
 PCB *timerfront = NULL;
 PCB *timerrear = NULL;
 PCB *Running;
 PCB *NextRunning;
 PCB *tempPCB;

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
	 if(readyfront==NULL&&readyrear==NULL){
		 readyfront = pcb;
		 readyrear = pcb;
		 //NextRunning = readyfront;
		 pcb->wakeuptime = 0;
	 }
	 else{
		 readyrear->next = pcb;
	     readyrear=pcb;
		 pcb->wakeuptime = 0;
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
	 {printf("can not find the process");
	 Z502_REG9++;}

	/*while(j == 1 ){
		 while(head!=0){
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
	 {m=1;}
	 }
	 if(m==1&&j==1){printf("can not find the process!");}*/
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

		printf("cunrrent running pid %d\n",Running->pid);

		head = readyfront;
		printf("readyqueue:");
		while(head!=NULL){
			
			printf("%d\t",head->pid);
			head=head->next;
		}
		printf("\n");
		head = timerfront;
		printf("timerqueue:");
		while(head!=NULL){
			
			printf("%d\t",head->pid);
			
			head=head->next;
		}
		printf("\n");
	   //READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,&LockResult);
	   if(Running != NULL){
		   Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &(Running->context) );
	   }
	   else
	   {CALL(Z502Idle());}
		//Z502Idle();

	 
}

void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;
    static BOOL        remove_this_in_your_code = TRUE;   /** TEMP **/
    static INT32       how_many_interrupt_entries = 0;    /** TEMP **/
	int                current_time;
	
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

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

    printf( "Fault_handler: Found vector type %d with value %d\n",
                        device_id, status );
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
    long                tmp;
	long                tmpid;
	void                *next_context;
	PCB                 *head;
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
             if(Pid < 20){
             if(pcb->Priority >0){
		       if(readyfront == NULL&&readyrear == NULL){
                 readyfront = pcb;
                 readyrear = pcb;
                 //*SystemCallData->Argument[4] = ERR_SUCCESS;
                 Z502_REG9 = ERR_SUCCESS;
				 pcb->pid = Pid;
				 pcb->next=NULL;
				 *SystemCallData->Argument[3] = Pid;
				 pcb->status=READY;
				 //pcb->context = (void *)SystemCallData->Argument[1];
				Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE );
				pcb->context = next_context;
				 Pid++;
                 }
               else if(readyfront!=NULL&&readyrear!=NULL){
				 if(checkPCBname(pcb) == 1){
					  readyrear->next = pcb;
                  //*SystemCallData->Argument[4] = ERR_SUCCESS;
                  Z502_REG9 = ERR_SUCCESS;
				  pcb->pid = Pid;
				  pcb->next=NULL;
				  pcb->status=READY;
				  //pcb->context = (void *)SystemCallData->Argument[1];
				  Z502MakeContext( &next_context, (void *)SystemCallData->Argument[1], USER_MODE );
				  pcb->context = next_context;

				 *SystemCallData->Argument[3] = Pid;
				 readyrear = pcb;
				 Pid++;}
				 else free(pcb);
                  }
			 }else free(pcb);
			 }
             else{
				 Z502_REG9++;
				 free(pcb);}
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
    int                 Prio = 1; 
	PCB *pcb = (PCB *)malloc(sizeof(PCB));
	char name[20] ="testbase";


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
    Z502MakeContext( &next_context, (void *)test1c, USER_MODE );
	pcb->context = next_context;
    Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &next_context );
}                                               // End of osInit
