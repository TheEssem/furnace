#ifndef _DISPATCH_H
#define _DISPATCH_H

enum DivDispatchCmds {
  DIV_CMD_NOTE_ON=0,
  DIV_CMD_NOTE_OFF,
  DIV_CMD_INSTRUMENT,
  DIV_CMD_VOLUME,
  DIV_CMD_PITCH_UP,
  DIV_CMD_PITCH_DOWN,
  DIV_CMD_PITCH_TO
};

struct DivCommand {
  DivDispatchCmds cmd;
};

struct DivDelayedCommand {
  int ticks;
  DivCommand cmd;
};

class DivEngine;

class DivDispatch {
  protected:
    DivEngine* parent;
  public:
    /**
     * the rate the samples are provided.
     * the engine shall resample to the output rate.
     */
    int rate;
    virtual void acquire(short& l, short& r);
    virtual int dispatch(DivCommand c);

    /**
     * initialize this DivDispatch.
     * @param parent the parent DivEngine.
     * @param channels the number of channels to acquire.
     * @param sugRate the suggested rate. this may change, so don't rely on it.
     * @return the number of channels allocated.
     */
    virtual int init(DivEngine* parent, int channels, int sugRate);
};
#endif