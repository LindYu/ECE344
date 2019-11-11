/*
/*
 * catlock.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use LOCKS/CV'S to solve the cat syncronization problem in
 * this file.
 */


/*
 *
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>
#include <curthread.h>
#include <machine/spl.h>


/*
 *
 * Constants
 *
 */

/*
 * Number of food bowls.
 */

#define NFOODBOWLS 2

/*
 * Number of cats.
 */

#define NCATS 6

/*
 * Number of mice.
 */

#define NMICE 2

/*
 * Condition variables and Locks
 */
struct lock *emptybowls[2];
struct cv *cateat;
struct cv *mouseeat;
struct lock *mutex, *finishmutex;
struct lock *mousecat;

int ncat, nmouse;

//int bowl1, bowl2;
int finished;

/*
 *
 * Function Definitions
 *
 */

/* who should be "cat" or "mouse" */
static void
lock_eat(const char *who, int num, int bowl, int iteration)
{
        kprintf("%s: %d starts eating: bowl %d, iteration %d\n", who, num,
                bowl, iteration);
        clocksleep(1);
        kprintf("%s: %d ends eating: bowl %d, iteration %d\n", who, num,
                bowl, iteration);
}

/*
 * catlock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS -
 *      1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
catlock(void * unusedpointer,
        unsigned long catnumber)
{
        /*
         * Avoid unused variable warnings.
         */

        (void) unusedpointer;
       // (void) catnumber;
       
    int i = 0;
    while (i < 4) {

        lock_acquire(mutex);
        while (nmouse!=0) {
            cv_wait(cateat, mutex);
        }
        lock_release(mutex);
        int bowl_choice = random()%2;
        lock_acquire(emptybowls[bowl_choice]);
        lock_acquire(mutex);
        ncat++;
        lock_release(mutex);
        
        lock_eat("cat", catnumber, bowl_choice+1, i);

        lock_acquire(mutex);
        ncat--;
        
        lock_release(emptybowls[bowl_choice]);

        lock_release(mutex);
        lock_acquire(finishmutex);
        cv_signal(cateat, finishmutex);
        
        cv_broadcast(mouseeat, finishmutex);
        i++;
        finished++;
        lock_release(finishmutex);
        
    }
}
    

/*
 * mouselock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds the mouse identifier from 0 to
 *              NMICE - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
mouselock(void * unusedpointer,
          unsigned long mousenumber)
{
        /*
         * Avoid unused variable warnings.
         */
        
        (void) unusedpointer;
//        (void) mousenumber;
//
    
    int i = 0;
    while (i < 4) {
        lock_acquire(mutex);
        while (ncat!=0) {
            cv_wait(mouseeat, mutex);
        }
        
        int bowl_choice = random()%2;
        lock_release(mutex);
        
        lock_acquire(emptybowls[bowl_choice]);
        lock_acquire(mutex);
        nmouse++;
        
        lock_release(mutex);
        lock_eat("mouse", mousenumber, bowl_choice+1, i);

        lock_acquire(mutex);
        nmouse--;

        lock_release(emptybowls[bowl_choice]);
        
        lock_release(mutex);

        lock_acquire(finishmutex);
        cv_signal(mouseeat, finishmutex);
        
        cv_broadcast(cateat, finishmutex);
        
        i++;
        finished++;
        lock_release(finishmutex);
    }

}


/*
 * catmouselock()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catlock() and mouselock() threads.  Change
 *      this code as necessary for your solution.
 */

int
catmouselock(int nargs,
             char ** args)
{
        int index, error;
   
        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;

    /*
     * Initialize global variables.
     */
    mutex = lock_create("mutex");
    assert(mutex!=NULL);
    emptybowls[0] = lock_create("bowl");
    emptybowls[1] = lock_create("bowl");
    assert(emptybowls!=NULL);
    finishmutex = lock_create("finish");
    assert(finishmutex!=NULL);
    cateat = cv_create("cat");
    assert(cateat!=NULL);
    mouseeat = cv_create("mouse");
    assert(mouseeat!=NULL);
    
    ncat = 0, nmouse = 0;
    
    finished = 0;
   
        /*
         * Start NCATS catlock() threads.
         */

        for (index = 0; index < NCATS; index++) {
           
                error = thread_fork("catlock thread",
                                    NULL,
                                    index,
                                    catlock,
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
                 
                        panic("catlock: thread_fork failed: %s\n",
                              strerror(error)
                              );
                }
        }

        /*
         * Start NMICE mouselock() threads.
         */

        for (index = 0; index < NMICE; index++) {
   
                error = thread_fork("mouselock thread",
                                    NULL,
                                    index,
                                    mouselock,
                                    NULL
                                    );
      
                /*
                 * panic() on error.
                 */

                if (error) {
         
                        panic("mouselock: thread_fork failed: %s\n",
                              strerror(error)
                              );
                }
        }
    
    while (finished < 4*(NMICE+NCATS)){
        thread_yield();
    }

    lock_destroy(emptybowls[0]);
    lock_destroy(emptybowls[1]);
    cv_destroy(cateat);
    cv_destroy(mouseeat);
    lock_destroy(finishmutex);
    lock_destroy(mutex);
    
        return 0;
}

/*
 * End of catlock.c
 */

