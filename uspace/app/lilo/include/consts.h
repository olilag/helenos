//
// Created by vik on 4/30/21.
//

#ifndef HELENOS_CONSTS_H
#define HELENOS_CONSTS_H

#define LINUX_O_ACCMODE         0003
#define LINUX_O_RDONLY            00
#define LINUX_O_WRONLY            01
#define LINUX_O_RDWR              02
#define LINUX_O_CREAT           0100
#define LINUX_O_EXCL            0200
#define LINUX_O_NOCTTY          0400
#define LINUX_O_TRUNC          01000
#define LINUX_O_APPEND         02000
#define LINUX_O_NONBLOCK       04000
#define LINUX_O_NDELAY       O_NONBLOCK
#define LINUX_O_SYNC        04010000
#define LINUX_O_FSYNC         O_SYNC
#define LINUX_O_ASYNC         020000

#endif  // HELENOS_CONSTS_H
