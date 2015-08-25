#ifndef __ZMODEM_DEFS_H__5OF6YBTIIO__
#define __ZMODEM_DEFS_H__5OF6YBTIIO__


/* Definitons from zmodem.h in the lrzsz package */

#define ZPAD '*'        /* 052 Padding character begins frames */
#define ZDLE 030        /* Ctrl-X Zmodem escape - `ala BISYNC DLE */
#define ZDLEE (ZDLE^0100)       /* Escaped ZDLE as transmitted */
#define ZBIN 'A'        /* Binary frame indicator */
#define ZHEX 'B'        /* HEX frame indicator */
#define ZBIN32 'C'      /* Binary frame with 32 bit FCS */

/* Frame types */
#define ZRQINIT 0       /* Request receive init */
#define ZRINIT  1       /* Receive init */
#define ZSINIT 2        /* Send init sequence (optional) */
#define ZACK 3          /* ACK to above */
#define ZFILE 4         /* File name from sender */
#define ZSKIP 5         /* To sender: skip this file */
#define ZNAK 6          /* Last packet was garbled */
#define ZABORT 7        /* Abort batch transfers */
#define ZFIN 8          /* Finish session */
#define ZRPOS 9         /* Resume data trans at this position */
#define ZDATA 10        /* Data packet(s) follow */
#define ZEOF 11         /* End of file */
#define ZFERR 12        /* Fatal Read or Write error Detected */
#define ZCRC 13         /* Request for file CRC and response */
#define ZCHALLENGE 14   /* Receiver's Challenge */
#define ZCOMPL 15       /* Request is complete */
#define ZCAN 16         /* Other end canned session with CAN*5 */
#define ZFREECNT 17     /* Request for free bytes on filesystem */
#define ZCOMMAND 18     /* Command from sending program */
#define ZSTDERR 19      /* Output to standard error, data follows */

/* ZDLE sequences */
#define ZCRCE 'h'       /* CRC next, frame ends, header packet follows */
#define ZCRCG 'i'       /* CRC next, frame continues nonstop */
#define ZCRCQ 'j'       /* CRC next, frame continues, ZACK expected */
#define ZCRCW 'k'       /* CRC next, ZACK expected, end of frame */
#define ZRUB0 'l'       /* Translate to rubout 0177 */
#define ZRUB1 'm'       /* Translate to rubout 0377 */

/* Bit Masks for ZRINIT flags byte ZF0 */
#define CANFDX  0x01    /* Rx can send and receive true FDX */
#define CANOVIO 0x02    /* Rx can receive data during disk I/O */
#define CANBRK  0x04    /* Rx can send a break signal */
#define CANCRY  0x08    /* Receiver can decrypt */
#define CANLZW  0x10    /* Receiver can uncompress */
#define CANFC32 0x20    /* Receiver can use 32 bit Frame Check */
#define ESCCTL  0x40    /* Receiver expects ctl chars to be escaped */
#define ESC8    0x80    /* Receiver expects 8th bit to be escaped */
/* Bit Masks for ZRINIT flags byze ZF1 */
#define ZF1_CANVHDR  0x01  /* Variable headers OK, unused in lrzsz */
#define ZF1_TIMESYNC 0x02 /* nonstandard, Receiver request timesync */

/* Parameters for ZFILE frame */
/* Conversion options one of these in ZF0 */
#define ZCBIN   1       /* Binary transfer - inhibit conversion */
#define ZCNL    2       /* Convert NL to local end of line convention */
#define ZCRESUM 3       /* Resume interrupted file transfer */
/* Management include options, one of these ored in ZF1 */
#define ZF1_ZMSKNOLOC   0x80 /* Skip file if not present at rx */
/* Management options, one of these ored in ZF1 */
#define ZF1_ZMMASK          0x1f /* Mask for the choices below */
#define ZF1_ZMNEWL         1 /* Transfer if source newer or longer */
#define ZF1_ZMCRC          2 /* Transfer if different file CRC or length */
#define ZF1_ZMAPND         3 /* Append contents to existing file (if any) */
#define ZF1_ZMCLOB         4 /* Replace existing file */
#define ZF1_ZMNEW          5 /* Transfer if source newer */
        /* Number 5 is alive ... */
#define ZF1_ZMDIFF         6 /* Transfer if dates or lengths different */
#define ZF1_ZMPROT         7 /* Protect destination file */
#define ZF1_ZMCHNG         8 /* Change filename if destination exists */

/* Transport options, one of these in ZF2 */
#define ZTLZW   1       /* Lempel-Ziv compression */
#define ZTCRYPT 2       /* Encryption */
#define ZTRLE   3       /* Run Length encoding */
/* Extended options for ZF3, bit encoded */
#define ZXSPARS 64      /* Encoding for sparse file operations */


#endif /* __ZMODEM_DEFS_H__5OF6YBTIIO__ */
