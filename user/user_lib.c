/*
 * The supporting library for applications.
 * Actually, supporting routines for applications are catalogued as the user
 * library. we don't do that in PKE to make the relationship between application
 * and user library more straightforward.
 */

#include "user_lib.h"
#include "util/types.h"
#include "util/snprintf.h"
#include "kernel/syscall.h"
#include "kernel/process.h"

uint64 do_user_call(uint64 sysnum, uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5, uint64 a6,
                    uint64 a7)
{
  int ret;

  // before invoking the syscall, arguments of do_user_call are already loaded into the argument
  // registers (a0-a7) of our (emulated) risc-v machine.
  asm volatile(
      "ecall\n"
      "sw a0, %0" // returns a 32-bit value
      : "=m"(ret)
      :
      : "memory");

  return ret;
}

//
// printu() supports user/lab1_1_helloworld.c
//
int printu(const char *s, ...)
{
  va_list vl;
  va_start(vl, s);

  char out[256]; // fixed buffer size.
  int res = vsnprintf(out, sizeof(out), s, vl);
  va_end(vl);
  const char *buf = out;
  size_t n = res < sizeof(out) ? res : sizeof(out);

  // make a syscall to implement the required functionality.
  return do_user_call(SYS_user_print, (uint64)buf, n, 0, 0, 0, 0, 0);
}

//
// applications need to call exit to quit execution.
//
int exit(int code)
{
  return do_user_call(SYS_user_exit, code, 0, 0, 0, 0, 0, 0);
}

//
// lib call to naive_malloc
//
void *naive_malloc()
{
  return (void *)do_user_call(SYS_user_allocate_page, 0, 0, 0, 0, 0, 0, 0);
}

//
// lib call to naive_free
//
void naive_free(void *va)
{
  do_user_call(SYS_user_free_page, (uint64)va, 0, 0, 0, 0, 0, 0);
}

//
// lib call to naive_fork
int fork()
{
  return do_user_call(SYS_user_fork, 0, 0, 0, 0, 0, 0, 0);
}

//
// lib call to yield
//
void yield()
{
  do_user_call(SYS_user_yield, 0, 0, 0, 0, 0, 0, 0);
}

// wait
int wait(int pid)
{
  //若wait系统调用返回WAIT_NOT_END，子进程尚未终止，则继续等待，释放cpu
  //若返回WAIT_PID_ILLEGAL,则说明pid不合法，返回-1
  //若返回值不是以上两种，则说明子进程已经正常终止，返回子进程的pid
  while(1)
  {
    int status = do_user_call(SYS_user_wait, pid, 0, 0, 0, 0, 0, 0);
    switch (status)
    {
    case WAIT_NOT_END:
      yield();
      break;
    case WAIT_PID_ILLEGAL:
      return -1;
    default:
      return status;
      break;
    }
  }
}