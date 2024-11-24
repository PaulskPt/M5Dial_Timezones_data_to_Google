/*
* Original version of this file found at:
* https://github.com/m5stack/M5Atom/blob/master/examples/ATOM_BASE/ATOM_SPK/PlayRawPCM/AtomSPK.h
* In Arduino M5Stack ESP32 library found in folder:
* C:\Users\<User>\Documents\Arduino\libraries\M5Atom\examples\ATOM_BASE\ATOM_SPK\PlayRawPCM
*
* 2024-10-06 Ported to M5Stack Atom Echo by @PaulskPt (Github)
* while renaming class name to ATOMECHOSPKR and renaming C++ files to AtomEchoSPKR.h and AtomEchoSPKR.cpp-
*/
#ifndef _ATOMECHOSPKR_H_
#define _ATOMECHOSPKR_H_

#include "AtomEchoSPKR.h"
#include "Arduino.h"
#include <driver/i2s.h>
#include "FS.h"

#define CONFIG_I2S_BCK_PIN     19
#define CONFIG_I2S_LRCK_PIN    33
#define CONFIG_I2S_DATA_PIN    22
#define CONFIG_I2S_DATA_IN_PIN 23

#define SPEAKER_I2S_NUMBER I2S_NUM_0

typedef struct beepParameters {
    int rate;
    int freq;
    int maxval;
    size_t time;
    // beepParameters() :rate(44100),freq(2000),maxval(10000),time(500){}
    // beepParameters(int rate,int freq,int maxval,size_t time)
    // :rate(rate),freq(freq),maxval(maxval),time(time){}
} beepParameters_t;

typedef struct {
    void *pAudioData;
    int length;
    bool freeFlag;
} audioParameters_t;

enum playType {
    kTypeNull = 0,
    kTypeAudio,
    kTypeBeep,
};

typedef struct {
    int type;
    void *dataptr;
} i2sQueueMsg_t;

typedef struct audioList {
    size_t _num;
    int type;
    void *dataptr;
    audioList *nextPtr;
} audioList_t;

// Added by @PaulskPt
typedef struct 
{
  int freq;
  int time_ms;
  int maxval;
  bool modal;
} beep;

const double sinmap[] = 
{ 
  0.0,
  0.01745240643728351,
  0.03489949670250097,
  0.05233595624294383,
  0.0697564737441253,
  0.08715574274765817,
  0.10452846326765346,
  0.12186934340514748,
  0.13917310096006544,
  0.15643446504023087,
  0.17364817766693033,
  0.1908089953765448,
  0.20791169081775931,
  0.224951054343865,
  0.24192189559966773,
  0.25881904510252074,
  0.27563735581699916,
  0.29237170472273677,
  0.3090169943749474,
  0.32556815445715664,
  0.3420201433256687,
  0.35836794954530027,
  0.374606593415912,
  0.3907311284892737,
  0.40673664307580015,
  0.42261826174069944,
  0.4383711467890774,
  0.45399049973954675,
  0.4694715627858908,
  0.48480962024633706,
  0.49999999999999994,
  0.5150380749100542,
  0.5299192642332049,
  0.5446390350150271,
  0.5591929034707469,
  0.573576436351046,
  0.5877852522924731,
  0.6018150231520483,
  0.6156614753256582,
  0.6293203910498374,
  0.6427876096865393,
  0.6560590289905072,
  0.6691306063588582,
  0.6819983600624985,
  0.6946583704589973,
  0.7071067811865476,
  0.7193398003386511,
  0.7313537016191705,
  0.7431448254773941,
  0.754709580222772,
  0.766044443118978,
  0.7771459614569708,
  0.788010753606722,
  0.7986355100472928,
  0.8090169943749475,
  0.8191520442889918,
  0.8290375725550417,
  0.8386705679454239,
  0.8480480961564261,
  0.8571673007021122,
  0.8660254037844386,
  0.8746197071393957,
  0.8829475928589269,
  0.8910065241883678,
  0.898794046299167,
  0.9063077870366499,
  0.9135454576426009,
  0.9205048534524403,
  0.9271838545667874,
  0.9335804264972017,
  0.9396926207859083,
  0.9455185755993167,
  0.9510565162951535,
  0.9563047559630354,
  0.9612616959383189,
  0.9659258262890683,
  0.9702957262759965,
  0.9743700647852352,
  0.9781476007338056,
  0.981627183447664,
  0.984807753012208,
  0.9876883405951378,
  0.9902680687415703,
  0.992546151641322,
  0.9945218953682733,
  0.9961946980917455,
  0.9975640502598242,
  0.9986295347545738,
  0.9993908270190958,
  0.9998476951563913,
  1.0,
  0.9998476951563913,
  0.9993908270190958,
  0.9986295347545738,
  0.9975640502598242,
  0.9961946980917455,
  0.9945218953682734,
  0.9925461516413221,
  0.9902680687415704,
  0.9876883405951377,
  0.984807753012208,
  0.981627183447664,
  0.9781476007338057,
  0.9743700647852352,
  0.9702957262759965,
  0.9659258262890683,
  0.9612616959383189,
  0.9563047559630355,
  0.9510565162951536,
  0.9455185755993168,
  0.9396926207859084,
  0.9335804264972017,
  0.9271838545667874,
  0.9205048534524404,
  0.913545457642601,
  0.90630778703665,
  0.8987940462991669,
  0.8910065241883679,
  0.8829475928589271,
  0.8746197071393959,
  0.8660254037844387,
  0.8571673007021123,
  0.8480480961564261,
  0.838670567945424,
  0.8290375725550417,
  0.819152044288992,
  0.8090169943749475,
  0.7986355100472927,
  0.788010753606722,
  0.777145961456971,
  0.766044443118978,
  0.7547095802227718,
  0.7431448254773942,
  0.7313537016191706,
  0.7193398003386514,
  0.7071067811865476,
  0.6946583704589971,
  0.6819983600624985,
  0.6691306063588583,
  0.6560590289905073,
  0.6427876096865395,
  0.6293203910498377,
  0.6156614753256584,
  0.6018150231520482,
  0.5877852522924732,
  0.5735764363510464,
  0.5591929034707469,
  0.5446390350150269,
  0.5299192642332049,
  0.5150380749100544,
  0.49999999999999994,
  0.48480962024633717,
  0.4694715627858911,
  0.45399049973954686,
  0.4383711467890773,
  0.4226182617406995,
  0.40673664307580043,
  0.39073112848927416,
  0.37460659341591224,
  0.3583679495453002,
  0.3420201433256689,
  0.32556815445715703,
  0.3090169943749475,
  0.29237170472273705,
  0.27563735581699966,
  0.258819045102521,
  0.24192189559966773,
  0.22495105434386478,
  0.20791169081775931,
  0.19080899537654497,
  0.17364817766693028,
  0.15643446504023098,
  0.13917310096006574,
  0.12186934340514755,
  0.10452846326765373,
  0.08715574274765864,
  0.06975647374412552,
  0.05233595624294381,
  0.0348994967025007,
  0.01745240643728344,
  0.0,
  -0.017452406437283192,
  -0.0348994967025009,
  -0.052335956242943564,
  -0.06975647374412483,
  -0.08715574274765794,
  -0.10452846326765305,
  -0.12186934340514774,
  -0.13917310096006552,
  -0.15643446504023073,
  -0.17364817766693047,
  -0.19080899537654472,
  -0.20791169081775907,
  -0.22495105434386498,
  -0.2419218955996675,
  -0.25881904510252035,
  -0.275637355816999,
  -0.2923717047227364,
  -0.30901699437494773,
  -0.32556815445715676,
  -0.34202014332566866,
  -0.35836794954530043,
  -0.374606593415912,
  -0.39073112848927355,
  -0.4067366430757998,
  -0.4226182617406993,
  -0.43837114678907707,
  -0.45399049973954625,
  -0.46947156278589086,
  -0.48480962024633695,
  -0.5000000000000001,
  -0.5150380749100542,
  -0.5299192642332048,
  -0.5446390350150271,
  -0.5591929034707467,
  -0.5735764363510458,
  -0.587785252292473,
  -0.601815023152048,
  -0.6156614753256578,
  -0.6293203910498376,
  -0.6427876096865393,
  -0.6560590289905074,
  -0.6691306063588582,
  -0.6819983600624984,
  -0.6946583704589973,
  -0.7071067811865475,
  -0.7193398003386509,
  -0.7313537016191701,
  -0.743144825477394,
  -0.7547095802227717,
  -0.7660444431189779,
  -0.7771459614569711,
  -0.7880107536067221,
  -0.7986355100472928,
  -0.8090169943749473,
  -0.8191520442889916,
  -0.8290375725550414,
  -0.8386705679454242,
  -0.848048096156426,
  -0.8571673007021121,
  -0.8660254037844385,
  -0.8746197071393959,
  -0.882947592858927,
  -0.8910065241883678,
  -0.8987940462991668,
  -0.9063077870366497,
  -0.913545457642601,
  -0.9205048534524403,
  -0.9271838545667873,
  -0.9335804264972016,
  -0.9396926207859082,
  -0.9455185755993168,
  -0.9510565162951535,
  -0.9563047559630353,
  -0.961261695938319,
  -0.9659258262890683,
  -0.9702957262759965,
  -0.9743700647852351,
  -0.9781476007338056,
  -0.9816271834476639,
  -0.984807753012208,
  -0.9876883405951377,
  -0.9902680687415704,
  -0.9925461516413221,
  -0.9945218953682734,
  -0.9961946980917455,
  -0.9975640502598242,
  -0.9986295347545738,
  -0.9993908270190957,
  -0.9998476951563913,
  -1.0,
  -0.9998476951563913,
  -0.9993908270190958,
  -0.9986295347545738,
  -0.9975640502598243,
  -0.9961946980917455,
  -0.9945218953682734,
  -0.992546151641322,
  -0.9902680687415704,
  -0.9876883405951378,
  -0.9848077530122081,
  -0.9816271834476641,
  -0.9781476007338058,
  -0.9743700647852352,
  -0.9702957262759966,
  -0.9659258262890682,
  -0.9612616959383188,
  -0.9563047559630354,
  -0.9510565162951536,
  -0.945518575599317,
  -0.9396926207859085,
  -0.9335804264972021,
  -0.9271838545667874,
  -0.9205048534524405,
  -0.9135454576426008,
  -0.9063077870366498,
  -0.898794046299167,
  -0.891006524188368,
  -0.8829475928589271,
  -0.8746197071393961,
  -0.8660254037844386,
  -0.8571673007021123,
  -0.8480480961564261,
  -0.8386705679454243,
  -0.8290375725550421,
  -0.8191520442889918,
  -0.8090169943749476,
  -0.798635510047293,
  -0.7880107536067218,
  -0.7771459614569708,
  -0.7660444431189781,
  -0.7547095802227722,
  -0.7431448254773946,
  -0.731353701619171,
  -0.7193398003386517,
  -0.7071067811865477,
  -0.6946583704589976,
  -0.6819983600624983,
  -0.6691306063588581,
  -0.6560590289905074,
  -0.6427876096865396,
  -0.6293203910498378,
  -0.6156614753256588,
  -0.6018150231520483,
  -0.5877852522924734,
  -0.5735764363510465,
  -0.5591929034707473,
  -0.544639035015027,
  -0.5299192642332058,
  -0.5150380749100545,
  -0.5000000000000004,
  -0.4848096202463369,
  -0.4694715627858908,
  -0.45399049973954697,
  -0.438371146789077,
  -0.4226182617407,
  -0.40673664307580015,
  -0.3907311284892747,
  -0.37460659341591235,
  -0.35836794954530077,
  -0.3420201433256686,
  -0.32556815445715753,
  -0.3090169943749477,
  -0.29237170472273627,
  -0.2756373558169998,
  -0.2588190451025207,
  -0.24192189559966787,
  -0.22495105434386534,
  -0.20791169081775987,
  -0.19080899537654467,
  -0.17364817766693127,
  -0.1564344650402311,
  -0.13917310096006588,
  -0.12186934340514811,
  -0.10452846326765342,
  -0.08715574274765832,
  -0.06975647374412476,
  -0.05233595624294437,
  -0.034899496702500823,
  -0.01745240643728445
};

class ATOMECHOSPKR {
   public:
    bool begin(int __rate = 44100);
    size_t playRAW(const uint8_t *__audioPtr, size_t __size,
                   bool __modal = false, bool freeFlag = true,
                   TickType_t __ticksToWait = portMAX_DELAY);
    size_t playBeep(beep __beep);
    size_t playBeep(int __freq = 2000, int __timems = 200, int __maxval = 10000,
                    bool __modal = false);
    void setVolume(int volume);  // Added by @PaulskPt

   private:
    size_t _rate      = 44100;
    size_t listLength = 0;
};

#endif