#define SCULL_MAJOR 0
#define SCULL_NR_DEVS 4
#define SCULL_QUANTUM 2048
#define SCULL_QSET 10


struct scull_qset {
        void **data;    /* data array (array of quantum) */
        struct scull_qset *next;
};

struct scull_dev {
        struct scull_qset *data;  /* Pointer ro first quantum set */
        int quantum;              /* the current quantum size */
        int qset;                 /* the current quantum array size */
        unsigned long size;       /* amount of data stored here */
        unsigned int access_key;   /* used by sculluid and scullpriv */
        struct semaphore sem;     /* mutual exclusion semaphore */
        struct cdev cdev;         /* Char device structure */
};
