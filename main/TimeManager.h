#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

class TimeManager {
public:
  void init(const char* timeZone);
  void runAdaptiveSync(); // This would be called in a dedicated task
  static void timeSyncNotificationCb(struct timeval *tv);
};

#endif // TIME_MANAGER_H
