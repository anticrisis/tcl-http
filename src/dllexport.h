#ifndef DLLEXPORT_H
#define DLLEXPORT_H

#if defined _WIN32 || defined __CYGWIN__

#ifdef __GNUC__
#define DllExport __attribute__((dllexport))
#else
#define DllExport __declspec(dllexport)
#endif

#else

#if __GNUC__ >= 4
#define DllExport __attribute__((visibility("default")))
#else
#define DllExport
#endif
#endif

#endif
