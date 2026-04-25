/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2026 tildearrow and contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "fileOpsCommon.h"
#include <numeric>

bool DivEngine::loadDMU(unsigned char *file, size_t len) {
  struct InvalidHeaderException {};
  bool success = false;
  char magic[32];

  bool isDMU2 = false;

  unsigned int seqLen, patLen, sampleLen, insLen, waveLen, sampleBytes;

  SafeReader reader = SafeReader(file, len);
  warnings = "";

  std::vector<int> patLens;

  struct DMUSequence {
    unsigned char pat[4];
    signed char transpose[4];
  };
  std::vector<DMUSequence> seq;

  struct DMUPatternRow {
    unsigned char note;
    unsigned char inst;
    unsigned char effect;
    unsigned char val2;
  };
  struct DMUPattern {
    DMUPatternRow rows[64];
  };
  std::vector<DMUPattern> patterns;

  struct DMUSampleInfo {
    unsigned int start;
    unsigned int end;
    unsigned int loop;
    String name;
  };
  std::vector<DMUSampleInfo> smpInfo;

  try {
    DivSong ds;
    ds.tuning = 436.0;
    ds.version = DIV_VERSION_DMU;

    // load here
    if (!reader.seek(0, SEEK_SET)) {
      logE("premature end of file!");
      lastError = "incomplete file";
      delete[] file;
      return false;
    }
    reader.read(magic, 24);

    if (memcmp(magic, DIV_DMU_MAGIC, 24) == 0) {
      isDMU2 = false;
    } else if (memcmp(magic, DIV_DMU2_MAGIC, 24) == 0) {
      isDMU2 = true;
    } else {
      logW("the magic isn't complete");
      throw EndOfFileException(&reader, reader.tell());
    }

    ds.systemLen = 1;
    ds.system[0] = DIV_SYSTEM_AMIGA;
    ds.systemVol[0] = 1.0f;
    ds.systemPan[0] = 0;
    ds.systemFlags[0].set("clockSel", 1); // PAL
    ds.systemFlags[0].set("stereoSep", 20);
    ds.systemFlags[0].set("chipType", 1);
    ds.systemName = "Amiga";

    if (isDMU2) {
      ds.systemLen = 2;
      ds.system[1] = DIV_SYSTEM_AMIGA;
      ds.systemVol[1] = 1.0f;
      ds.systemPan[1] = -0.2f;
      ds.systemFlags[1].set("clockSel", 1); // PAL
      ds.systemFlags[1].set("stereoSep", 0);
      ds.systemFlags[1].set("chipType", 1);
      ds.systemName = "Amiga";
    }

    reader.readS_BE(); // unused data?
    patLen = reader.readS_BE();

    logD("patLen: %d", patLen);

    // per-subsong pattern lengths
    patLens.reserve(8);
    for (int i = 0; i < 8; i++) {
      patLens.push_back(reader.readI_BE());
    }
    seqLen = patLens[0];

    logD("patLens: %d %d %d %d %d %d %d %d", patLens[0], patLens[1], patLens[2],
         patLens[3], patLens[4], patLens[5], patLens[6], patLens[7]);

    insLen = reader.readI_BE();
    if (insLen < 1 || insLen > 64) {
      logW("ins length is not above 0 or below 64 (%d)", insLen);
    }
    waveLen = reader.readI_BE();
    if (waveLen < 1 || waveLen > 32) {
      logW("wave length is not above 0 or below 32 (%d)", waveLen);
    }
    sampleLen = reader.readI_BE();
    sampleBytes = reader.readI_BE();

    logD("insLen: %d", insLen);
    logD("waveLen: %d", waveLen);
    logD("sampleLen: %d", sampleLen);
    logD("sampleBytes: %d", sampleBytes);

    // predefine subsongs
    for (int i = 0; i < 7; i++) {
      ds.subsong.push_back(new DivSubSong);
    }

    std::vector<bool> subsongLoop;
    subsongLoop.reserve(8);
    std::vector<int> loopPoints;
    loopPoints.reserve(8);
    for (int i = 0; i < 8; i++) {
      subsongLoop.push_back(reader.readC());
      loopPoints.push_back(reader.readC());
      if (loopPoints[i] > 126) {
        logW("subsong %d loop point is higher than 126! (%d)", i,
             loopPoints[i]);
      }
      ds.subsong[i]->speeds.val[0] = reader.readC();
      if (ds.subsong[i]->speeds.val[0] < 1 ||
          ds.subsong[i]->speeds.val[0] > 15) {
        logW("subsong %d speed is out of range! (%d)",
             ds.subsong[i]->speeds.val[0]);
      }
      ds.subsong[i]->speeds.val[1] = ds.subsong[i]->speeds.val[0];
      ds.subsong[i]->speeds.len = 2;
      char seqVal = reader.readC();
      if (seqVal != patLens[i]) {
        logW("subsong %d pattern counts do not match! (%d, %d)", i, seqVal,
             patLens[i]);
      }
      ds.subsong[i]->name = reader.readString(12);
    }

    logD("loopPoints: %d %d %d %d %d %d %d %d", loopPoints[0], loopPoints[1],
         loopPoints[2], loopPoints[3], loopPoints[4], loopPoints[5],
         loopPoints[6], loopPoints[7]);

    // orders
    unsigned int seqCount =
        std::accumulate(patLens.begin(), patLens.end(), 0, std::plus<int>());
    logD("reading sequences... (%d)", seqCount);
    seq.reserve(seqCount);
    for (unsigned int i = 0; i < seqCount; i++) {
      DMUSequence s;
      for (int j = 0; j < 4; j++) {
        s.pat[j] = reader.readC();
        s.transpose[j] = reader.readC();
      }
      seq.push_back(s);
      logV("%.2x | %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x", i, s.pat[0],
           s.transpose[0], s.pat[1], s.transpose[1], s.pat[2], s.transpose[2],
           s.pat[3], s.transpose[3]);
    }

    std::vector<unsigned int> insVolIndex;
    std::vector<unsigned int> insPitchIndex;
    std::vector<int> insFinetuneIndex;
    std::vector<unsigned int> insArpIndex;

    // instruments
    logD("reading instruments... (%d)", insLen);
    ds.ins.reserve(insLen);
    insVolIndex.reserve(insLen);
    insPitchIndex.reserve(insLen);
    insFinetuneIndex.reserve(insLen);
    insArpIndex.reserve(insLen);
    for (unsigned int i = 0; i < insLen; i++) {
      DivInstrument *ins = new DivInstrument;

      ins->type = DIV_INS_AMIGA;
      ins->name = fmt::sprintf("Instrument %d", i);

      int waveNum = reader.readC();
      int waveLen = reader.readC();

      if (waveNum >= 0x20) {
        ins->amiga.useSample = true;
        ins->amiga.initSample = waveNum - 0x20;
      } else {
        ins->amiga.useWave = true;
        ins->amiga.waveLen = (waveLen * 2) - 1;
      }

      // volume sequence
      insVolIndex.push_back(reader.readC());
      ins->std.volMacro.len = 128;
      ins->std.volMacro.speed = reader.readC();

      int arpNum = reader.readC();
      insArpIndex.push_back(arpNum);

      // pitch sequence
      unsigned int pitch = reader.readC();
      reader.readC(); // unknown
      unsigned int pitchDelay = reader.readC();
      int finetune = reader.readC();
      unsigned int pitchLoop = reader.readC();
      unsigned int pitchSpeed = reader.readC();
      insPitchIndex.push_back(pitch);
      if (pitch > 0) {
        ins->std.pitchMacro.len = 128;
        ins->std.pitchMacro.open = true;
        ins->std.pitchMacro.delay = pitchDelay;
        ins->std.pitchMacro.loop = pitchLoop;
        ins->std.pitchMacro.speed = pitchSpeed;
      } else if (finetune > 0) {
        ins->std.pitchMacro.len = 1;
        ins->std.pitchMacro.open = true;
        ins->std.pitchMacro.val[0] = -(finetune * 8);
      }

      insFinetuneIndex.push_back(finetune);

      // figure these out
      int effectNum = reader.readC();
      int srcWave1 = reader.readC();
      /*int srcWave2 =*/reader.readC();
      int effectSpd = reader.readC();

      if (effectNum == 7) {
        ins->ws.enabled = true;
        ins->ws.effect = DIV_WS_NEGATIVE_OVERLAY;
        ins->ws.wave1 = waveNum;
        ins->ws.wave2 = srcWave1;
        ins->ws.speed = effectSpd;
      }

      if (effectNum == 9) {
        ins->ws.enabled = true;
        ins->ws.effect = DIV_WS_OVERLAY;
        ins->ws.wave1 = waveNum;
        ins->ws.wave2 = srcWave1;
        ins->ws.speed = effectSpd;
      }

      if (!ins->ws.enabled && waveNum < 0x20) {
        ins->std.waveMacro.len = 1;
        ins->std.waveMacro.val[0] = waveNum;
        ins->std.waveMacro.open = true;
      }

      ins->std.volMacro.loop = reader.readC() ? 0 : 0xff;

      ds.ins.push_back(ins);
    }
    ds.insLen = (int)ds.ins.size();

    // wavetables
    logD("reading wavetables... (%d)", waveLen);
    ds.wave.reserve(waveLen);
    for (unsigned int i = 0; i < waveLen; i++) {
      DivWavetable *w = new DivWavetable;
      w->min = 0;
      w->max = 255;
      w->len = 128;
      for (unsigned int j = 0; j < 128; j++) {
        w->data[j] = ~reader.readC() + 128;
      }

      for (unsigned int j = 0; j < insLen; j++) {
        if (i == insVolIndex[j]) {
          unsigned int adjustedIdx = 0;
          bool started = false;
          for (unsigned int k = 0; k < 128; k++) {
            int val = w->data[k] / 4;
            if (val == 0 && !started) {
              continue;
            } else {
              started = true;
              ds.ins[j]->std.volMacro.val[adjustedIdx] = val;
              adjustedIdx++;
            }
          }
          if (adjustedIdx == 0) {
            ds.ins[j]->std.volMacro.len = 1;
            ds.ins[j]->std.volMacro.val[0] = 0;
          } else {
            ds.ins[j]->std.volMacro.len = adjustedIdx;
          }
        }

        if (i == insPitchIndex[j] && ds.ins[j]->std.pitchMacro.len == 128) {
          int finetune = insFinetuneIndex[j] * 8;
          for (unsigned int k = 0; k < 128; k++) {
            ds.ins[j]->std.pitchMacro.val[k] =
                ((w->data[k] - 128) * 4) - finetune;
          }
        }
      }

      ds.wave.push_back(w);
    }
    ds.waveLen = (int)ds.wave.size();

    // sample info
    logD("samples: (%x)", sampleLen);
    for (unsigned int i = 0; i < sampleLen; i++) {
      DMUSampleInfo smp;
      smp.start = reader.readI_BE();
      smp.end = reader.readI_BE();
      smp.loop = reader.readI_BE();
      smp.name = reader.readString(20);
      logD("- %d: %d, %d (%d)", i, smp.start, smp.end, smp.loop);
      smpInfo.push_back(smp);
    }

    // convert
    ds.subsong[0]->ordersLen = seqLen;
    ds.subsong[0]->patLen = 64;
    ds.subsong[0]->hz = 50;

    if (isDMU2) {
      ds.subsong[0]->chanShow[3] = false;
      ds.subsong[0]->chanShowChanOsc[3] = false;

      for (int i = 4; i < 8; i++) {
        ds.subsong[0]->chanName[i] = fmt::sprintf("Channel %d", i);
        ds.subsong[0]->chanShortName[i] = fmt::sprintf("CH%d", i);
      }
    }

    // patterns
    logD("reading patterns... (%d)", patLen);
    patterns.reserve(patLen);
    for (unsigned int i = 0; i < patLen; i++) {
      DMUPattern pat;
      logV("- pattern %d", i);
      for (int j = 0; j < 64; j++) {
        pat.rows[j].note = reader.readC();
        pat.rows[j].inst = reader.readC();
        pat.rows[j].effect = reader.readC();
        pat.rows[j].val2 = reader.readC();
      }
      patterns.push_back(pat);
    }

    for (unsigned int i = 0; i < seqLen; i++) {
      int lastRow = 63;

      for (int j = 0; j < 4 * ds.systemLen; j++) {
        int realCh = j % 4;
        bool inSubsong = j > 3;
        DMUSequence s = seq[inSubsong ? seqLen + i : i];
        ds.subsong[0]->orders.ord[j][i] = i;
        DivPattern *p = ds.subsong[0]->pat[j].getPattern(i, true);

        unsigned char sPat = s.pat[realCh];
        char sTrans = s.transpose[realCh];

        DMUPattern dmuPat = patterns[sPat];

        int lastEffect = 0;

        for (int k = 0; k < 64; k++) {
          if (lastEffect == 0x41 || lastEffect == 0x42) {
            // p->newData[k][DIV_PAT_FX(0)] = 0xea;
            p->newData[k][DIV_PAT_FX(0)] = 0xf6;
            p->newData[k][DIV_PAT_FXVAL(0)] = 0;
            lastEffect = 0;
          }

          if (dmuPat.rows[k].note == 0)
            continue;

          if (dmuPat.rows[k].note > 56) {
            logW("note is outside range! (%d)", dmuPat.rows[k].note);
          }

          short note = dmuPat.rows[k].note + 73;
          if (sTrans != 0) {
            note += sTrans;
          }
          p->newData[k][DIV_PAT_NOTE] = note;
          p->newData[k][DIV_PAT_INS] = dmuPat.rows[k].inst - 1;

          if (dmuPat.rows[k].effect != 0) {
            /*if (dmuPat.rows[k].effect < 0x40) {
              lastEffect = dmuPat.rows[k].effect;
              char amount = dmuPat.rows[k].val2;
              if (amount < 0) {
                p->newData[k][DIV_PAT_FX(0)] = 0xee;
                p->newData[k][DIV_PAT_FXVAL(0)] = -dmuPat.rows[k].val2;
                // p->newData[k][DIV_PAT_FX(0)] = 0x02;
                // p->newData[k][DIV_PAT_FXVAL(0)] = -dmuPat.rows[k].val2;
              } else {
                p->newData[k][DIV_PAT_FX(0)] = 0xed;
                p->newData[k][DIV_PAT_FXVAL(0)] = 0;
                // p->newData[k][DIV_PAT_FX(0)] = 0x01;
                // p->newData[k][DIV_PAT_FXVAL(0)] = dmuPat.rows[k].val2;
              }
            }*/

            lastEffect = dmuPat.rows[k].effect;

            if (dmuPat.rows[k].effect < 0x40) {
              logV("unhandled pitch bend!");
            }

            if (dmuPat.rows[k].effect == 0x40) {
              logV("unhandled effect reinit disable!");
            }

            if (dmuPat.rows[k].effect == 0x41) {
              // p->newData[k][DIV_PAT_FX(0)] = 0xea;
              // p->newData[k][DIV_PAT_FXVAL(0)] = 1;
              p->newData[k][DIV_PAT_FX(0)] = 0xf5;
              p->newData[k][DIV_PAT_FXVAL(0)] = 0;
            }

            // todo: handle wavetable effects for this and 0x40?
            if (dmuPat.rows[k].effect == 0x42) {
              // p->newData[k][DIV_PAT_FX(0)] = 0xea;
              // p->newData[k][DIV_PAT_FXVAL(0)] = 1;
              p->newData[k][DIV_PAT_FX(0)] = 0xf5;
              p->newData[k][DIV_PAT_FXVAL(0)] = 0;
            }

            if (dmuPat.rows[k].effect == 0x43) {
              lastRow = dmuPat.rows[k].val2 - 1;
              p->newData[lastRow][DIV_PAT_FX(0)] = 0x0d;
              p->newData[lastRow][DIV_PAT_FXVAL(0)] = 0;
            }

            if (dmuPat.rows[k].effect == 0x44) {
              p->newData[k][DIV_PAT_FX(0)] = 0x0f;
              p->newData[k][DIV_PAT_FXVAL(0)] = dmuPat.rows[k].val2;
            }

            if (dmuPat.rows[k].effect == 0x45) {
              p->newData[k][DIV_PAT_FX(0)] = 0x10;
              p->newData[k][DIV_PAT_FXVAL(0)] = 1;
            }

            if (dmuPat.rows[k].effect == 0x46) {
              p->newData[k][DIV_PAT_FX(0)] = 0x10;
              p->newData[k][DIV_PAT_FXVAL(0)] = 0;
            }

            if (dmuPat.rows[k].effect == 0x47) {
              logV("unhandled rapid led!");
            }

            if (dmuPat.rows[k].effect == 0x48) {
              logV("unhandled waveform wait!");
            }

            if (dmuPat.rows[k].effect == 0x49) {
              logV("unhandled arp change!");
            }

            if (dmuPat.rows[k].effect == 0x4a) {
              p->newData[k][DIV_PAT_FX(0)] = 0x03;
              p->newData[k][DIV_PAT_FXVAL(0)] = dmuPat.rows[k].val2;
            }

            if (dmuPat.rows[k].effect == 0x4b) {
              p->newData[k][DIV_PAT_FX(0)] = 0x09;
              p->newData[k][DIV_PAT_FXVAL(0)] =
                  ds.subsong[0]->speeds.val[0] + dmuPat.rows[k].val2 + 128;
            }

            if (dmuPat.rows[k].effect > 0xfb) {
              logW("nonexistent pattern effect detected! (%x)",
                   dmuPat.rows[k].effect);
            }
          }
        }

        if (subsongLoop[0] && i == (seqLen - 1) && j == (isDMU2 ? 7 : 3)) {
          p->newData[lastRow][DIV_PAT_FX(0)] = 0x0b;
          p->newData[lastRow][DIV_PAT_FXVAL(0)] = loopPoints[0];
        }
      }
    }

    // samples
    logD("reading samples... (%d)", sampleLen);
    unsigned int samplePtr = reader.tell();
    for (unsigned int i = 0; i < sampleLen; i++) {
      DMUSampleInfo info = smpInfo[i];
      DivSample *s = new DivSample;
      s->depth = DIV_SAMPLE_DEPTH_8BIT;
      s->name = info.name;

      unsigned int len = info.end - info.start;
      if (len > 0) {
        s->init(len);
      }

      if (info.loop > 1) {
        s->loopStart = info.loop - info.start;
        s->loopEnd = len;
        s->loop = (s->loopStart >= 0) && (s->loopEnd >= 0);
      }
      if (!reader.seek(samplePtr + info.start, SEEK_SET)) {
        logE("premature end of file!");
        lastError = "incomplete file";
        delete[] file;
        return false;
      }
      reader.read(s->data8, len);
      ds.sample.push_back(s);
    }
    ds.sampleLen = (int)ds.sample.size();

    // arps
    logD("reading arps...");
    if (!reader.seek(samplePtr + sampleBytes, SEEK_SET)) {
      logE("premature end of file!");
      lastError = "incomplete file";
      delete[] file;
      return false;
    }
    std::vector<std::vector<char>> arps;
    arps.reserve(8);
    for (unsigned int i = 0; i < 8; i++) {
      char arp[32];
      reader.read(arp, 32);
      std::vector<char> arpVec(arp, arp + 32);
      arps.push_back(arpVec);
    }
    for (unsigned int i = 0; i < insLen; i++) {
      unsigned int arp = insArpIndex[i];
      if (arp > 0) {
        ds.ins[i]->std.arpMacro.len = 32;
        ds.ins[i]->std.arpMacro.open = true;
        for (unsigned int j = 0; j < 32; j++) {
          ds.ins[i]->std.arpMacro.val[j] = arps[arp][j];
        }
      }
    }

    // optimize
    ds.subsong[0]->optimizePatterns();
    ds.subsong[0]->rearrangePatterns();

    ds.initDefaultSystemChans();
    ds.recalcChans();

    // finalize
    if (active)
      quitDispatch();
    BUSY_BEGIN_SOFT;
    saveLock.lock();
    song.unload();
    song = ds;
    hasLoadedSomething = true;
    changeSong(0);
    saveLock.unlock();
    BUSY_END;
    if (active) {
      initDispatch();
      BUSY_BEGIN;
      renderSamples();
      reset();
      BUSY_END;
    }
    success = true;
  } catch (EndOfFileException &e) {
    // logE("premature end of file!");
    lastError = "incomplete file";
  } catch (InvalidHeaderException &e) {
    // logE("invalid header!");
    lastError = "invalid header!";
  }
  return success;
}
