#pragma once

#include <psp2/types.h>

class Thread
{
public:
    Thread(const char *name = "");
    ~Thread();

    int start();

    unsigned int stackSize() const;
    void setStackSize(unsigned int size);

protected:
    virtual void run() = 0;

    int getThreadId();

private:
    const char *m_name = "";
    int m_thid = -1;
    unsigned int m_stackSize = 0x1000;

    static int threadEntry(SceSize args, void *argp);
};

inline int Thread::getThreadId() {
    return m_thid;
}
