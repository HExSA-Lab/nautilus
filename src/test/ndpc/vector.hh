#ifndef _vector_
#define _vector_

#include <new>

template <typename T> class Vector : public Collection<T> {
private:
    int numalloced;
    int numused;
    T *data;

protected:
    virtual void resize(int newalloc) { 
	if (newalloc <= numalloced) { 
	    return ;
	} else {
	    newalloc = newalloc + newalloc/2;  // headroom
	    T * newdata = new T [newalloc];
	    int i;
	    for (i=0;i<numused;i++) { newdata[i]=data[i]; }
	    delete [] data;
	    data = newdata;
	    numalloced = newalloc;
	}
    }

public:
    Vector() : Collection<T>() { numalloced=1024; data = new T[numalloced]; numused=0; };
    Vector(int numitems) : Collection<T>(numitems) { numalloced=numitems; data = new T[numalloced]; numused=0; };
    Vector(const Vector<T> &rhs) : Collection<T>(rhs) { numalloced=numused=rhs.numused; data = new T[numalloced]; for (int i=0;i<numused;i++) { data[i]=rhs.data[i]; } };
    Vector(const Collection<T> &rhs) : Collection<T>(rhs) { numalloced=numused=rhs.numitems(); data = new T[numalloced]; for (int i=0;i<numused;i++) { data[i]=rhs[i]; } };
    
    virtual ~Vector() { delete [] data; };

    virtual int numitems() const { return numused; };

    Collection<T> & operator=(const Collection<T> &rhs) { return * (new(this) Vector(rhs)); }
   
    virtual T const & operator[](const int offset) const { 
#ifndef NDPC_NAUTILUS_KERNEL
if ((offset+1)>numused) { throw Collection<T>::outofbounds; } else {return data[offset];}
#else
 return data[offset];
#endif
 };

    virtual T & operator[](const int offset) { resize(offset+1); if ((offset+1)>numused) {numused=offset+1;} return data[offset]; };


};


#endif
