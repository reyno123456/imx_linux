#ifndef _COMMON_H
#define _COMMON_H

#define DEBUG

#ifdef DEBUG
  #define DEBUG_INFO(fmt, args...)  fprintf(stderr, fmt, ##args) 
  #define DEBUG_ERROR(fmt, args...) fprintf(stderr, fmt"%s\t%s\t%d\n", ##args, __FILE__,__FUNCTION__,__LINE__)
#else
  #define DEBUG_INFO(fmt, args...) 
  #define DEBUG_ERROR(fmt, args...)
#endif        

#define RET_SUCCESS                 0
#define RET_FAILED                  -1

#endif
