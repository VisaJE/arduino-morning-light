#include <Servo.h>
#include <avr/wdt.h>

constexpr int CTRL = 6;
constexpr int PIEZOPIN = A0;

constexpr int LIGHTON = 65;
constexpr int CENTERED = 85;
constexpr int NOTIFY = 80;
constexpr int LIGHTOFF = 2*CENTERED - LIGHTON;

constexpr int minAng = 5;
constexpr int maxAng = 175;


#define time_t unsigned long


// Average array definition

struct Node
{
    float value;
    Node* newer;
};

class AverageArray
{
        int leng;
        int realleng;

        Node* first;
        Node* last;
    public:
        float total;
        float average() {return total / realleng;}

        int Length() {return realleng;}

        AverageArray(int len = 0, float total=0)
        {
            realleng = 0;
            leng = len;
            if (len <= 0)
            {
                leng = 0;
            }
            this->total = total;

            last = NULL;
            first = NULL;
        }

        ~AverageArray()
        {
            Node* rem = last;
            while (rem != NULL)
            {
                last = rem->newer;
                delete rem;
                rem = last;
            }
        }

        void updateArray(float value)
        {
            if (leng == 0) return;
            if (realleng < leng)
            {
                Node* nod = new Node;
                nod->value = value;
                if (first != NULL)
                    first->newer = nod;
                first = nod;
            }

            if (last != NULL && leng == realleng)
            {
                total -= last->value;

                auto slast = last->newer;

                last->value = value; // Reusing last Node
                last->newer = NULL;
                first->newer = last;
                first = last;
                last = slast;
            }
            else if (last == NULL)
            {
                last = first;
            }

            total += first->value;
            if (realleng < leng)
                    realleng += 1;
        }
};


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
  friend Time operator- (const Time& t1, const Time& t2)
  {
    return Time(t1.asMs() - t2.asMs());
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
bool pressDir = true; // Stores the sign of the voltage delta.
bool pressed = false;
bool firstSet = false; // Set the initial press dir

bool offDone;
bool onDone;
AverageArray dbuffer;

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
  Serial.begin(115200);
  wdt_enable(WDTO_4S);

  moveservo(CENTERED);
  offDone = true;
  onDone = true;

  dbuffer = AverageArray(5);
  int sensorValue = analogRead(PIEZOPIN);
  float voltage = sensorValue * (5.0 / 1023.0);
  dbuffer.updateArray(voltage); // Keep track of long time average

  auto timenow = Time(millis());

}

constexpr float sensitivity = 0.9;


void loop() {
  auto timenow = Time(millis());

  int sensorValue = analogRead(PIEZOPIN);
  float voltage = sensorValue * (5.0 / 1023.0);

  dbuffer.updateArray(voltage); // Keep track of long time average
  float dif = voltage - dbuffer.average();
  float chg = abs(dif);

  if (chg > sensitivity)
  {
    bool dir = (bool)(dif/chg + 1);

    if (!firstSet)
    {
      pressDir = !dir;
      firstSet = true;
    }

    if (pressDir != dir)
    {
      if (!pressed)
      {
        pressT = Time(millis());
        pressed = true;
      }
      else
      {
        int pressLength = (int)((timenow.asMs() - pressT.asMs()) / 1000);
        lightT = timenow + wakeDelta + pressLength * wakeDeltaDelta;
        offT = timenow + offWait;

        //Serial.print("Currently ");
        //timenow.print();
        //Serial.print("Light on at ");
        //lightT.print();
        //Serial.print("presslength was ");
        //Serial.println(pressLength);

        for (int i = 0; i < pressLength + 1; i++)
        {
          moveservo(NOTIFY, 100);
          moveservo(CENTERED, 250);
        }
        offDone = false;
        onDone = false;
        pressed = false;
      }
    }
    pressDir = dir;
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

  Serial.print(voltage);
  Serial.print(",");
  Serial.print(dbuffer.average());
  Serial.print(",");
  Serial.println(voltage - dbuffer.average());
  //Serial.print(pressed);
  delay(250);
}


