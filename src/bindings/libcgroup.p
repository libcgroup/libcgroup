%module libcgroup
%{
#include "libcgroup.h"
%}

#ifdef  __cplusplus
#define __BEGIN_DECLS   extern "C" {
#define __END_DECLS     }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

%include typemaps.i
%include cpointer.i
%pointer_functions(int, intp);
%typemap (in) void** (void *temp) {
        void *arg;
        if ((arg = PyCObject_AsVoidPtr($input)) != NULL) {
                $1 = &arg;
        } else
                $1 = &temp;
}

%typemap (argout) void** {
        PyObject *obj = PyCObject_FromVoidPtr(*$1, NULL);
        $result = PyTuple_Pack(2, $result, obj);
}

