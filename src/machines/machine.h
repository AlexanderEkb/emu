#ifndef __MACHINE_MACHINE_H__
#define __MACHINE_MACHINE_H__

class Machine_t
{
  public:
    virtual char * getName();
    virtual void * getMisc();

    virtual void init();
    virtual void run();
    virtual void shutdown();
};

#endif /* __MACHINE_MACHINE_H__ */