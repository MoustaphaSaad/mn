
#ifndef HOT_RELOAD_LIB_EXPORT_H
#define HOT_RELOAD_LIB_EXPORT_H

#ifdef HOT_RELOAD_LIB_STATIC_DEFINE
#  define HOT_RELOAD_LIB_EXPORT
#  define HOT_RELOAD_LIB_NO_EXPORT
#else
#  ifndef HOT_RELOAD_LIB_EXPORT
#    ifdef hot_reload_lib_EXPORTS
        /* We are building this library */
#      define HOT_RELOAD_LIB_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define HOT_RELOAD_LIB_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef HOT_RELOAD_LIB_NO_EXPORT
#    define HOT_RELOAD_LIB_NO_EXPORT 
#  endif
#endif

#ifndef HOT_RELOAD_LIB_DEPRECATED
#  define HOT_RELOAD_LIB_DEPRECATED __declspec(deprecated)
#endif

#ifndef HOT_RELOAD_LIB_DEPRECATED_EXPORT
#  define HOT_RELOAD_LIB_DEPRECATED_EXPORT HOT_RELOAD_LIB_EXPORT HOT_RELOAD_LIB_DEPRECATED
#endif

#ifndef HOT_RELOAD_LIB_DEPRECATED_NO_EXPORT
#  define HOT_RELOAD_LIB_DEPRECATED_NO_EXPORT HOT_RELOAD_LIB_NO_EXPORT HOT_RELOAD_LIB_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef HOT_RELOAD_LIB_NO_DEPRECATED
#    define HOT_RELOAD_LIB_NO_DEPRECATED
#  endif
#endif

#endif /* HOT_RELOAD_LIB_EXPORT_H */
