// this file manages symbol visibility macros.
// because we dont want other applications to waste time looking for symbols they can't access through headers
// the default visibility is set to hidden in the CMakeLists file. All symbols in public headers (this directory)
// should (probably) be marked with default visibility to explicitly expose them in the .so/.dll

// the resulting macro from this blob is SOSIMPLE_API

// in your build (YES YOUR BUILD):
// add -DSOSIMPLE_SO if sosimple is a shared object
// add -DSOSIMPLE_BUILD if the shared object is being built
// you don't need to add anything for static linking

#if defined _WIN32 || defined __CYGWIN__
  #define SOSIMPLE_DLL_IMPORT __declspec(dllimport)
  #define SOSIMPLE_DLL_EXPORT __declspec(dllexport)
  #define SOSIMPLE_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define SOSIMPLE_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define SOSIMPLE_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define SOSIMPLE_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define SOSIMPLE_DLL_IMPORT
    #define SOSIMPLE_DLL_EXPORT
    #define SOSIMPLE_DLL_LOCAL
  #endif
#endif
#ifdef SOSIMPLE_SO // sosimple is built/used as shared object
  #ifdef SOSIMPLE_BUILD // is sosimple currently compiling?
    #define SOSIMPLE_API SOSIMPLE_DLL_EXPORT
  #else
    #define SOSIMPLE_API SOSIMPLE_DLL_IMPORT
  #endif
  #define SOSIMPLE_LOCAL SOSIMPLE_DLL_LOCAL
#else // sosimple is built/used as static lib
  #define SOSIMPLE_API
  #define SOSIMPLE_LOCAL
#endif
