#include <Servo.h>
#include <avr/wdt.h>

constexpr int CTRL = 6;
constexpr int SWITCHPIN = 7;

constexpr int LIGHTON = 65;
constexpr int CENTERED = 85;
constexpr int NOTIFY = 80;
constexpr int LIGHTOFF = 2*CENTERED - LIGHTON;

constexpr int minAng = 5;
constexpr int maxAng = 175;


#define time_t unsigned long


class Time
{
  private:
    time_t ms;
    time_t mins;
    time_t hs;

  public:
    time_t get_ms()
    {
      normalize();
      return ms;
    }
    time_t get_mins()
    {
      normalize();
      return mins;
    }
    time_t get_hs()
    {
      normalize();
      return hs;
    }

    void normalize()
    {
      if (ms >= 60000)
      {
        time_t delta = ms % 60000;
        mins += (ms - delta) / 60000;

        time_t deltam = mins % 60;
        hs += (mins - deltam) / 60;

        ms = delta;
        mins = deltam;
      }
    }

  Time(time_t millisecs=0,time_t minutes=0, time_t hours=0):
      ms(millisecs), mins(minutes), hs(hours)
  {
    normalize();
  }
  ~Time() {}

    void addMs(time_t millisecs)
  {
    ms += millisecs;
    //normalize(); // Stupid implementation
  }

  time_t asMs()
  const {
    return ms + 60000 * (mins + hs * 60);
  }

  friend Time operator+ (const Time& t1, const Time& t2)
  {
    return Time(t1.ms + t2.ms, t1.mins + t2.mins, t1.hs + t2.hs);
  }
  friend Time operator* (const int& sc, const Time& t2)
  {
    return Time(sc * t2.ms, sc * t2.mins, sc * t2.hs);
  }

    void print()
    {
      normalize();
      Serial.print("Time ");
      Serial.print(hs);
      Serial.print("h ");
      Serial.print(mins);
      Serial.print("min ");
      Serial.print(ms/1000);
      Serial.println("s");
    }
    // Only one minute accuracy
    friend bool operator== (const Time& t1, const Time& t2)
    {
      return t1.hs == t2.hs && t1.mins == t2.mins;
    }
    friend bool operator!= (const Time& t1, const Time& t2)
    {
      return !(t1 == t2);
    }

  friend bool operator> (const Time& t1, const Time& t2)
  {
    return t1.asMs() > t2.asMs();
  }
  friend bool operator< (const Time& t1, const Time& t2)
  {
    return t1.asMs() < t2.asMs();
  }

};


const Time wakeDelta = Time(0, 15, 8);
const Time wakeDeltaDelta = Time(0, 15, 0);

const Time offWait = Time(10000);

Servo myservo;  // create servo object to control a servo
bool pressed = false;

bool offDone;
bool onDone;

Time lightT;
Time pressT;
Time offT;

int curpos = -1;
void moveservo(int pos, int del=500)
{
  if (curpos != pos)
  {
    myservo.attach(CTRL);
    if (pos > minAng)
    {
      if (pos < maxAng)
        myservo.write(pos);
      else
        myservo.write(maxAng);
    }
    else
      myservo.write(minAng);
    delay(del);
    myservo.detach();
    curpos = pos;
  }
}


void setup() {
  //Serial.begin(115200);
  wdt_enable(WDTO_4S);

  moveservo(CENTERED);
  offDone = true;
  onDone = true;

  pinMode(SWITCHPIN, INPUT_PULLUP);
  auto timenow = Time(millis());

}

constexpr float sensitivity= 0.6;


void loop() {
  auto timenow = Time(millis());

  auto switchPos = digitalRead(SWITCHPIN) == LOW;

  if (pressed != switchPos)
  {

    if (switchPos)
    {
      pressT = Time(millis());
      pressed = true;
    }
    else
    {
      int pressLength = (int)((timenow.asMs() - pressT.asMs()) / 1000);
      lightT = timenow + wakeDelta + pressLength * wakeDeltaDelta;
      offT = timenow + offWait;

      for (int i = 0; i < pressLength + 1; i++)
      {
        moveservo(NOTIFY, 100);
        moveservo(CENTERED, 250);
      }
      offDone = false;
      onDone = false;
      pressed = false;
    }
    pressed = switchPos;
  }


  // Actions
  if (!onDone && timenow > lightT)
  {
    moveservo(LIGHTON);
    onDone = true;
  }
  else if (!offDone && timenow > offT)
  {
    moveservo(LIGHTOFF);
    offDone = true;
  }
  else
    moveservo(CENTERED);

  wdt_reset();

  delay(250);
}


