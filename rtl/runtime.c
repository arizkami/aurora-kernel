/* Minimal C runtime implementations for freestanding build */
#include "../aurora.h"
#ifndef va_start
#include <stdarg.h>
#endif

size_t strlen(const char* s){ const char* p=s; while(*p) ++p; return (size_t)(p-s);} 
int strcmp(const char* a,const char* b){ while(*a && (*a==*b)){++a;++b;} return *(const unsigned char*)a-*(const unsigned char*)b; }
int strncmp(const char* a,const char* b,size_t n){ for(size_t i=0;i<n;i++){ if(a[i]!=b[i]||!a[i]||!b[i]) return (unsigned char)a[i]-(unsigned char)b[i]; } return 0; }
char* strcpy(char* d,const char* s){ char* r=d; while((*d++=*s++)); return r; }
char* strncpy(char* d,const char* s,size_t n){ size_t i=0; for(; i<n && s[i]; ++i) d[i]=s[i]; for(; i<n; ++i) d[i]='\0'; return d; }
void* memset(void* p,int v,size_t n){ unsigned char* d=(unsigned char*)p; for(size_t i=0;i<n;i++) d[i]=(unsigned char)v; return p; }
void* memcpy(void* d,const void* s,size_t n){ unsigned char* dd=(unsigned char*)d; const unsigned char* ss=(const unsigned char*)s; for(size_t i=0;i<n;i++) dd[i]=ss[i]; return d; }
int memcmp(const void* a,const void* b,size_t n){ const unsigned char* pa=a; const unsigned char* pb=b; for(size_t i=0;i<n;i++){ if(pa[i]!=pb[i]) return pa[i]-pb[i]; } return 0; }
void* memmove(void* dest,const void* src,size_t n){ unsigned char* d=(unsigned char*)dest; const unsigned char* s=(const unsigned char*)src; if(d==s||n==0) return dest; if(d<s){ for(size_t i=0;i<n;i++) d[i]=s[i]; } else { for(size_t i=n;i>0;i--) d[i-1]=s[i-1]; } return dest; }
int abs(int x){ return x<0?-x:x; }
