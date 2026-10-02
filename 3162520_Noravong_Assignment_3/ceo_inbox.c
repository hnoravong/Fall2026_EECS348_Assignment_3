/*****************************************************************************
 * Program:       EECS 348 Assignment 3 - CEO Email Priority Queue
 *
 * Description:   Manages a busy CEO's email inbox as a priority queue.
 *                 The priority queue is implemented from scratch as a
 *                 MaxHeap using a LIST-BASED (dynamically growing array)
 *                 representation -- no built-in heap / priority_queue
 *                 library is used anywhere in this file.
 *
 *                 Emails are prioritized first by sender category, in this
 *                 order (highest priority first):
 *                     1. Boss
 *                     2. Subordinate
 *                     3. Peer
 *                     4. ImportantPerson
 *                     5. OtherPerson
 *                 Within the same category, the NEWEST email (by date) is
 *                 given higher priority than an older email from the same
 *                 category.
 *
 * Commands read (one per line) from standard input, or from a file passed
 * as argv[1]:
 *     EMAIL <sender category>,<subject line>,<date MM-DD-YYYY>
 *     NEXT     -> peek at (display) the highest priority email, but do NOT
 *                 remove it from the queue
 *     READ     -> remove ("read") the highest priority email from the
 *                 queue without displaying it
 *     COUNT    -> display how many unread emails remain in the queue
 *
 * Output:        Text printed to the terminal for NEXT and COUNT commands,
 *                 formatted per the assignment specification.
 *
 * Collaborators:  None
 * Other sources:  Claude and ChatGPT
 * Author:         Hunter Noravong
 * Creation date:  9/28/2026
 * Revision date:  9/1/2026
 * Revisions:      Final version 
 ****************************************************************************/

#include <stdio.h>      /* for fgets, printf, fopen, FILE, etc. */
#include <stdlib.h>     /* for malloc, realloc, free, exit */
#include <string.h>     /* for strcmp, strcpy, strtok, strlen, strcspn */
#include <ctype.h>      /* for isspace, used when trimming lines */

/* -------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------
 */
#define MAX_LINE_LEN      512   /* max characters we expect on one input line */
#define MAX_CATEGORY_LEN   32   /* max characters in a sender-category string */
#define MAX_SUBJECT_LEN   256   /* max characters in a subject line */
#define MAX_DATE_LEN       16   /* "MM-DD-YYYY" plus a little slack */
#define INITIAL_CAPACITY   16   /* starting capacity of the heap's array */

/* -------------------------------------------------------------------------
 * Email: one record in the CEO's inbox.
 * -------------------------------------------------------------------------
 */
typedef struct {
    char category[MAX_CATEGORY_LEN]; /* "Boss", "Subordinate", "Peer",       */
                                      /* "ImportantPerson", or "OtherPerson" */
    char subject[MAX_SUBJECT_LEN];   /* the email's subject line             */
    char date[MAX_DATE_LEN];         /* original date string, MM-DD-YYYY     */
    long dateSortKey;                /* date converted to YYYYMMDD as an int */
                                      /* so bigger number == more recent      */
} Email;

/* -------------------------------------------------------------------------
 * MaxHeap: a list-based (dynamic array) binary max-heap of Email records.
 * The element at index 0 is always the current highest-priority email.
 * -------------------------------------------------------------------------
 */
typedef struct {
    Email *items;      /* pointer to a heap-allocated array of Email        */
    int size;          /* number of emails currently stored in the heap     */
    int capacity;      /* current allocated size of the 'items' array       */
} MaxHeap;

/* =========================================================================
 * Helper functions: turn raw input strings into values we can compare
 * =========================================================================
 */

/* categoryRank: maps a sender-category string to an integer rank where a
 * SMALLER number means HIGHER priority (Boss = 0 is read first, and
 * OtherPerson = 4 is read last). Unknown categories are treated as the
 * lowest possible priority so the program never crashes on bad input. */
int categoryRank(const char *category) {
    if (strcmp(category, "Boss") == 0)            return 0; /* highest */
    if (strcmp(category, "Subordinate") == 0)      return 1;
    if (strcmp(category, "Peer") == 0)             return 2;
    if (strcmp(category, "ImportantPerson") == 0)  return 3;
    if (strcmp(category, "OtherPerson") == 0)      return 4; /* lowest */
    return 5; /* unrecognized category -> treat as even lower priority */
}

/* parseDateToSortKey: converts a "MM-DD-YYYY" string into a single integer
 * of the form YYYYMMDD (e.g., "12-20-2024" -> 20241220). Representing the
 * date this way lets us compare two dates with a single ">" on plain
 * integers -- a larger integer always means a later (more recent) date. */
long parseDateToSortKey(const char *date) {
    int month = 0, day = 0, year = 0;              /* fields to fill in     */
    /* sscanf reads the three numeric fields out of "MM-DD-YYYY" */
    sscanf(date, "%d-%d-%d", &month, &day, &year);
    return (long) year * 10000L + (long) month * 100L + (long) day;
}

/* isHigherPriority: returns 1 if email 'a' should be read BEFORE email 'b',
 * and 0 otherwise. This single function defines the entire ordering rule
 * used by the heap, so all of the "who goes first" logic lives in one
 * place. */
int isHigherPriority(const Email *a, const Email *b) {
    int rankA = categoryRank(a->category);   /* lower rank == more urgent  */
    int rankB = categoryRank(b->category);

    if (rankA != rankB) {
        return rankA < rankB;      /* smaller category rank wins outright */
    }
    /* Same category: the NEWER date (larger sort key) wins. */
    return a->dateSortKey > b->dateSortKey;
}

/* =========================================================================
 * MaxHeap implementation (list/array based, written from scratch)
 * =========================================================================
 */

/* heapInit: allocates the heap's backing array and sets size to 0. Must be
 * called once before the heap is used. */
void heapInit(MaxHeap *heap) {
    heap->items = (Email *) malloc(sizeof(Email) * INITIAL_CAPACITY); /* allocate initial storage */
    heap->size = 0;                       /* heap starts out empty         */
    heap->capacity = INITIAL_CAPACITY;    /* remember how much room we have */
}

/* heapFree: releases the heap's backing array. Call once at program exit. */
void heapFree(MaxHeap *heap) {
    free(heap->items);   /* give the array's memory back to the system     */
    heap->items = NULL;  /* avoid leaving a dangling pointer                */
    heap->size = 0;
    heap->capacity = 0;
}

/* heapEnsureCapacity: doubles the backing array's size whenever the heap
 * becomes full, so heapPush never runs out of room. */
void heapEnsureCapacity(MaxHeap *heap) {
    if (heap->size == heap->capacity) {              /* array is full      */
        heap->capacity *= 2;                          /* double the room    */
        heap->items = (Email *) realloc(heap->items, sizeof(Email) * heap->capacity);
    }
}

/* swapEmails: exchanges the contents of two Email slots in the array. Used
 * while sifting an element up or down the heap. */
void swapEmails(Email *a, Email *b) {
    Email temp = *a;   /* copy a into a scratch variable */
    *a = *b;           /* copy b's contents into a's slot */
    *b = temp;         /* copy the original a into b's slot */
}

/* siftUp: after appending a new element at index 'i' (the end of the
 * array), repeatedly swap it with its parent while it has higher priority
 * than its parent, restoring the max-heap property from the bottom up. */
void siftUp(MaxHeap *heap, int i) {
    while (i > 0) {                              /* stop once we reach the root */
        int parent = (i - 1) / 2;                /* array-based heap parent index */
        if (isHigherPriority(&heap->items[i], &heap->items[parent])) {
            swapEmails(&heap->items[i], &heap->items[parent]); /* bubble up */
            i = parent;                          /* continue from the parent's slot */
        } else {
            break;                               /* heap property restored, stop */
        }
    }
}

/* siftDown: after moving the last element into the root (index 0) to fill
 * the hole left by a removal, repeatedly swap it with its higher-priority
 * child until the max-heap property is restored from the top down. */
void siftDown(MaxHeap *heap, int i) {
    while (1) {
        int left = 2 * i + 1;        /* index of left child, if it exists  */
        int right = 2 * i + 2;       /* index of right child, if it exists */
        int best = i;                /* assume current node is highest for now */

        if (left < heap->size && isHigherPriority(&heap->items[left], &heap->items[best])) {
            best = left;              /* left child has higher priority */
        }
        if (right < heap->size && isHigherPriority(&heap->items[right], &heap->items[best])) {
            best = right;             /* right child has higher priority than current best */
        }
        if (best == i) {
            break;                   /* neither child outranks us -> heap property holds */
        }
        swapEmails(&heap->items[i], &heap->items[best]); /* push the smaller-priority item down */
        i = best;                    /* continue sifting down from the child's old slot */
    }
}

/* heapPush: inserts a new Email into the heap while keeping the max-heap
 * property intact (append then sift up). */
void heapPush(MaxHeap *heap, Email email) {
    heapEnsureCapacity(heap);              /* grow the array if it's full  */
    heap->items[heap->size] = email;       /* place the new email at the end */
    siftUp(heap, heap->size);              /* restore heap order upward   */
    heap->size++;                          /* one more email is now stored */
}

/* heapPeek: returns a pointer to the highest-priority email without
 * removing it, or NULL if the heap is empty. */
Email *heapPeek(MaxHeap *heap) {
    if (heap->size == 0) {
        return NULL;         /* nothing to peek at */
    }
    return &heap->items[0];  /* root of the heap is always the max element */
}

/* heapPop: removes the highest-priority email from the heap (does not
 * return/print it -- the caller decides what, if anything, to display).
 * Does nothing if the heap is already empty. */
void heapPop(MaxHeap *heap) {
    if (heap->size == 0) {
        return;                                  /* nothing to remove       */
    }
    heap->items[0] = heap->items[heap->size - 1]; /* move last item to root */
    heap->size--;                                 /* shrink the heap by one */
    if (heap->size > 0) {
        siftDown(heap, 0);                        /* restore heap property  */
    }
}

/* =========================================================================
 * Input parsing helpers
 * =========================================================================
 */

/* trimNewline: removes a trailing '\n' and/or '\r' left on the line by
 * fgets, and also strips a trailing '\r' for files created on Windows. */
void trimNewline(char *line) {
    size_t len = strlen(line);                    /* length before trimming */
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[len - 1] = '\0';                      /* chop off the last char */
        len--;
    }
}

/* parseEmailLine: parses the text AFTER "EMAIL " (e.g.,
 * "Boss,Never Mind,01-03-2025") into an Email struct. Fields are split on
 * commas; the subject may contain spaces (but not commas, per the spec). */
void parseEmailLine(const char *rest, Email *outEmail) {
    char buffer[MAX_LINE_LEN];
    strncpy(buffer, rest, MAX_LINE_LEN - 1);   /* copy so strtok can modify it */
    buffer[MAX_LINE_LEN - 1] = '\0';

    char *categoryTok = strtok(buffer, ",");   /* 1st field: sender category  */
    char *subjectTok  = strtok(NULL, ",");     /* 2nd field: subject line     */
    char *dateTok     = strtok(NULL, ",");     /* 3rd field: date MM-DD-YYYY  */

    /* Copy each parsed field into the Email struct, guarding against a
     * malformed line that is missing a field. */
    strncpy(outEmail->category, categoryTok ? categoryTok : "", MAX_CATEGORY_LEN - 1);
    outEmail->category[MAX_CATEGORY_LEN - 1] = '\0';

    strncpy(outEmail->subject, subjectTok ? subjectTok : "", MAX_SUBJECT_LEN - 1);
    outEmail->subject[MAX_SUBJECT_LEN - 1] = '\0';

    strncpy(outEmail->date, dateTok ? dateTok : "", MAX_DATE_LEN - 1);
    outEmail->date[MAX_DATE_LEN - 1] = '\0';

    outEmail->dateSortKey = parseDateToSortKey(outEmail->date); /* precompute sort key */
}

/* =========================================================================
 * Command handlers
 * =========================================================================
 */

/* handleNext: implements the NEXT command -- display the highest-priority
 * email WITHOUT removing it from the queue. */
void handleNext(MaxHeap *heap) {
    Email *top = heapPeek(heap);          /* look at, but don't remove, the top email */
    if (top == NULL) {
        printf("No emails to read.\n");   /* handle the empty-queue edge case */
        return;
    }
    printf("Next email:\n");
    printf("Sender: %s\n", top->category);
    printf("Subject: %s\n", top->subject);
    printf("Date: %s\n", top->date);
}

/* handleRead: implements the READ command -- remove the highest-priority
 * email from the queue (the CEO "dealt with it"), without printing it. */
void handleRead(MaxHeap *heap) {
    heapPop(heap);  /* heapPop is already a no-op if the heap is empty */
}

/* handleCount: implements the COUNT command -- print how many unread
 * emails remain in the queue. */
void handleCount(MaxHeap *heap) {
    printf("There are %d emails to read.\n", heap->size);
}

/* =========================================================================
 * main: reads commands one line at a time and dispatches to the correct
 * handler above.
 * =========================================================================
 */
int main(int argc, char *argv[]) {
    MaxHeap heap;
    heapInit(&heap);                     /* set up an empty MaxHeap        */

    /* Read from a file named on the command line if one was given,
     * otherwise fall back to standard input (so the grader can run this
     * as either "./ceo_inbox tests.txt" or "./ceo_inbox < tests.txt"). */
    FILE *input = stdin;                 /* default: read from stdin       */
    if (argc > 1) {
        input = fopen(argv[1], "r");     /* try to open the given filename */
        if (input == NULL) {
            fprintf(stderr, "Error: could not open file '%s'\n", argv[1]);
            heapFree(&heap);
            return 1;                    /* exit with a failure status     */
        }
    }

    char line[MAX_LINE_LEN];
    /* Read the input one line at a time until end-of-file. */
    while (fgets(line, sizeof(line), input) != NULL) {
        trimNewline(line);               /* strip trailing \n / \r          */

        if (line[0] == '\0') {
            continue;                    /* silently skip blank lines       */
        }

        if (strncmp(line, "EMAIL ", 6) == 0) {
            Email newEmail;
            parseEmailLine(line + 6, &newEmail);  /* skip past "EMAIL "      */
            heapPush(&heap, newEmail);             /* add it to the queue    */
        } else if (strcmp(line, "NEXT") == 0) {
            handleNext(&heap);
        } else if (strcmp(line, "READ") == 0) {
            handleRead(&heap);
        } else if (strcmp(line, "COUNT") == 0) {
            handleCount(&heap);
        }
        /* Any other/unrecognized line is silently ignored so the program
         * never crashes on unexpected input. */
    }

    if (input != stdin) {
        fclose(input);       /* close the file if we opened one ourselves   */
    }
    heapFree(&heap);         /* release the heap's memory before exiting    */
    return 0;
}