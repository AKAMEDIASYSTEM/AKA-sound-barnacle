
/*
   here's where we have a routine that samples an analog channel
   that is connected to a mic amp board

   we have a large buffer we use to store the last X samples
   could ALSO have a 60-element Neopixel showing average noise level over the last 60 minutes?

   we need two scopes of time here: sub-second sampling that establishes average noise level
   and minute-to-minute summaries of that noise

   we could use millis() to very naively bin samples by modulus-of-seconds, ie:
   take sample
   check millis() and put sample in the appropriate bin
*/

void listen() {
  unsigned long startMillis = millis(); // Start of sample window
  unsigned int peakToPeak = 0;   // peak-to-peak level
  int sample;

  while (millis() - startMillis < sampleWindow)
  {
    sample = analogRead(MIC_PIN);
    if (sample > currentMax) {
      currentMax = sample;
    }
    if (sample < currentMin) {
      currentMin = sample;
    }
  }
} // end of listen


void everyMinute() {
  Serial.println("Timer fired, currentMin and currentMax next");
  Serial.println(currentMin);
  Serial.println(currentMax);
  for (int i = NUM_PIXELS - 1; i > 0; i--) {
    // make room for new entry at 0 by shifting everything to the next index over
    pastMin[i] = pastMin[i - 1];
    pastMax[i] = pastMax[i - 1];
    levels[i] = levels[i - 1];
    r[i] = r[i - 1];
    g[i] = g[i - 1];
    b[i] = b[i - 1];
  }
  pastMin[0] = currentMin;
  pastMax[0] = currentMax;
  levels[0] = abs(currentMax - currentMin);
  Serial.print("Levels for this minute is ");
  Serial.println(levels[0]);
  r[0] = (levels[0] < 196 ) ? 0 : levels[0];
  g[0] = (levels[0] < 128 ) ? levels[0] : 0;
  b[0] = (levels[0] < 64 ) ? 255 : 0;

  currentMin = 512;
  currentMax = 512;
}


float floatmap(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

