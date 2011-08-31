/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef _GENERIC_STATS_H
#define _GENERIC_STATS_H

// To use generic statistics:
//   * create a structure or class to hold your counters, 
//     * use stats_entry_abs    for counters that are absolute values (i.e. number of jobs running)
//     * use stats_entry_recent for counters that always increase (i.e. number of jobs that have finished)
//   * use Add() or Set() methods of the counters to update the counters in your code
//       these methods will automatically keep track of total value, as well as windowed
//       values and/or peak values. 
//   * create a const array of GenericStatsPubItem structures, one for each Attribute
//   * use generic_stats_PublishToClassAd passing the GenericStatsPubItem and also your counters class 
//     if you use the GENERIC_STATS_ENTRY macros to initialize the GenericStatsPubItem structures,
//     counters will be published as ClassAd attributes using their field names.
//
// For example:
//
// typedef struct MyStats {
//     time_t                  UpdateTime                     
//     stats_entry_abs<int>    JobsRunning;  // keep track of Peak value as well as absolete value
//     stats_entry_recent<int> JobsRun;      // keep track of Recent values as well as accumulated value
//
//     void Publish(ClassAd & ad) const;
// } MyStats;
// 
// static const GenericStatsPubItem MyStatsPub[] = {
//   GENERIC_STATS_PUB_TYPE(MyStats, "My", UpdateTime, AS_ABSTIME, time_t), // publish "MyUpdateTime"
//   GENERIC_STATS_PUB(MyStats, "My", JobsRunning,      AS_COUNT), // publish "MyJobsRunning"
//   GENERIC_STATS_PUB_PEAK(MyStats, "My", JobsRunning, AS_COUNT), // publish "MyJobsRunningPeak" from JobsRunning
//   GENERIC_STATS_PUB(MyStats, "My", JobsRun,          AS_COUNT), // publish "MyJobsRun"
//   GENERIC_STATS_PUB_RECENT(MyStats, "My", JobsRun,   AS_COUNT), // publish "RecentMyJobsRun" from JobsRun
//   };
//
// void MyStats::Publish(ClassAd & ad) const
// {
//    generic_stats_PublishToClassAd(ad, MyStatsPub, COUNTOF(MyStatsPub), (const char *)this);
// }
//
// include an instance of MyStats in the class to be measured
// 
//  class MyClass {
//      ...
//      MyStats stats;
//      ...
//      void MyMethod() {
//          stats.UpdateTime = time(NULL);
//
//           // this to update the absolute jobs running counter
//          stats.JobsRunning.Set(jobs_running);
//           // or this
//          stats.JobsRunning = jobs_running; 
//
//           // this to increment the jobs that have run counter
//          stats.JobsRun.Add(1);
//           // or this 
//          stats.JobsRun += 1;
//
//      }
//
//      void PublishMyStats(ClassAd & ad) const {
//          stats.Publish(ad);
//      }
//  };
//    
//

// this structure is used to describe one field of a statistics structure
// so that we can generically update and publish
//
/*
typedef struct _generic_stats_entry {
   char * pattr;  // name to be used when publishing the value
   int    units;  // field type, AS_COUNT, AS_ABSTIME, etc. plus IS_xx, IF_xx flags
   int    off;    // offset to statistics data value from start of data set
   int    siz;    // size of statistics data value
   int    off2;  // if non-zero, indicates that this value is baked down from a timed_queue.
   } GenericStatsEntry;
*/

/*
typedef void (*fn_generic_stats_publish)(const char * me, ClassAd & ad, const char * pattr);
typedef struct _generic_stats_pubitem {
   char * pattr;  // name to be used when publishing the value
   int    units;  // one or more of AS_COUNT, etc and IS_RINGBUF, etc
   int    off;    // offset of the stats_entry_xxx field within the containing class/struct
   fn_generic_stats_publish pub; // static method for publishing
   } GenericStatsPubItem;
*/

// this is used to identify the fundamental type of a statistics entry so
// that we can use generic data driven code to publish and clear collections
// of statistics. It works with the template specialization below to let us
// capture the type T inside a template expansion as an integer value. 
//
enum {
   STATS_ENTRY_TYPE_INT32 = 1,
   STATS_ENTRY_TYPE_INT64 = 2,
   STATS_ENTRY_TYPE_FLOAT = 1 | 4,
   STATS_ENTRY_TYPE_DOUBLE = 2 | 4,
   STATS_ENTRY_TYPE_UNSIGNED = 8,
   STATS_ENTRY_TYPE_UINT32 = STATS_ENTRY_TYPE_INT32 | STATS_ENTRY_TYPE_UNSIGNED,
   STATS_ENTRY_TYPE_UINT64 = STATS_ENTRY_TYPE_INT64 | STATS_ENTRY_TYPE_UNSIGNED,
   };

template <class T> struct stats_entry_type  {static const int id = 0;};
template<> struct stats_entry_type<int>     {static const int id = STATS_ENTRY_TYPE_INT32; };
template<> struct stats_entry_type<int64_t> {static const int id = STATS_ENTRY_TYPE_INT64; };
template<> struct stats_entry_type<unsigned int> {static const int id = STATS_ENTRY_TYPE_UINT32; };
template<> struct stats_entry_type<uint64_t>{static const int id = STATS_ENTRY_TYPE_UINT64; };
template<> struct stats_entry_type<float>   {static const int id = STATS_ENTRY_TYPE_FLOAT; };
template<> struct stats_entry_type<double>  {static const int id = STATS_ENTRY_TYPE_DOUBLE; };

// These enums are for the units field of GenericStatEntry
enum {
   AS_COUNT   = 0,      // an int or int64 count of something
   AS_ABSTIME = 0x10,   // a time_t absoute timestamp (i.e. from time(NULL))
   AS_RELTIME = 0x20,   // a time_t time duration

   // the fundamental type is determined by the compiler and mapped to 
   // the stats_entry_type<T>::id constants, which fit in the low 4 bits
   AS_FUNDAMENTAL_TYPE_MASK = 0x000F, // mask to get STATS_ENTRY_TYPE_xxx
   AS_TYPE_MASK   = 0x00FF, // mask the units field with this to get AS_xxx 

   // bits between 0x8000 and 0x0100 identify the probe class
   IS_CLASS_MASK  = 0xFF00, // 
   IS_CLS_COUNT   = 0x0000, // is stats_entry_count (has simple value)
   IS_CLS_ABS     = 0x0100, // is stats_entry_abs (has max)
   IS_CLS_PROBE   = 0x0200, // is stats_entry_probe class (has min/max/avg/stddev)
   IS_RECENT      = 0x0400, // is stats_entry_recent class (recent value derived from a ring_buffer)
   IS_RECENTTQ    = 0x0500, // is stats_entry_tq class (recent value derived from a timed_queue)
   IS_RCT         = 0x0600, // is stats_recent_counter_timer 
   IS_REPROBE     = 0x0700, // is stats_entry_recent<Probe> class
   IS_HISTOGRAM   = 0x0800, // is stats_entry_histgram class

   // values above AS_TYPE_MASK are flags
   //
   IF_NONZERO    = 0x10000, // only publish non-zero values.
   IF_NEVER      = 0x20000, // set this flag to disable publishing of the item.
   IF_PUBMASK    = IF_NEVER | IF_NONZERO, // flags that affect publication
   };


// Generic class for a ring buffer.  
// 
// A ring buffer does not grow except via the SetSize() method.
// Push() advances the location of the head through the buffer until 
// it gets to the last element at which point it wraps around to the first element.  
// 
// Once the size of the buffer is set, buffer entries can be accessed with the [] operator
// where [0] accesses the current entry, and [1-cItems] accesses the last (oldest) entry. 
// once the buffer is full, [1] also access the oldest entry because of the way the ring 
// buffer wraps around.
//
// the Add() method is used to add to the head element, it does not advance through the buffer.
//
// So after SetSize(6) and 6 or more Push() calls, we have 
//
//   pbuf:[aaa][bbb][ccc][ddd][eee][fff]
//              ^    ^
//              head tail
//   this[0] returns bbb
//
// After Add(2) we have
//
//   pbuf:[aaa][bbb+2][ccc][ddd][eee][fff]
//              ^      ^
//              head   tail
//   this[0] returns bbb+2
// 
// After Advance() or Push(0) we have
//
//   pbuf:[aaa][bbb][0  ][ddd][eee][fff]
//                   ^    ^
//                   head tail
//   this[0] returns 0
// 
//
template <class T> class ring_buffer {
public:
   ring_buffer(int cSize=0) : cMax(0), cAlloc(0), ixHead(0), cItems(0), pbuf(0) {
      if (cSize > 0) {
         pbuf = new T[cSize];
         cMax = cAlloc = cSize;
         }
      };
   ~ring_buffer() { delete[] pbuf; };
   int cMax;   // the maximum number of items in the ring, may be less than cAlloc but never more
   int cAlloc; // the allocation size of pbuf, should be 0 if pbuf is NULL
   int ixHead; // index of the head item (item[0]) within the ring buffer, advances with each Push
   int cItems; // number of items currently in the ring buffer, grows with each Push() until it hits cMax
   T*  pbuf;   // allocated buffer for the ring.

   T& operator[](int ix) { 
      if ( ! pbuf || ! cMax) return pbuf[0]; // yes, we do want to segfault if pbuf==NULL
      return pbuf[(ixHead+ix+cMax) % cMax];
   }

   int Length() const { return cItems; } 
   int MaxSize() const { return cMax; }
   bool empty() const { return cItems == 0; }

   bool Clear() {
      bool ret = cItems > 0;
      ixHead = 0;
      cItems = 0;
      return ret;
   }

   void Free() {
      ixHead = 0;
      cItems = 0;
      cMax = 0;
      cAlloc = 0;
      delete[] pbuf;
   }

   T Sum() {
      T tot(0);
      for (int ix = 0; ix > (0 - cItems); --ix)
         tot += (*this)[ix];
      return tot;
   }

   bool SetSize(int cSize) {

      if (cSize < 0) return false;

      // if current items are outside of the new ring buffer from [0 to cSize]
      // then we have to copy items, so we might as well allocate a new buffer
      // even if we are shrinking.
      bool fMustCopy = (cItems > 0) && (ixHead > cSize || ixHead - cItems + 1 < 0);

      if ((cSize > cAlloc) || fMustCopy) {
         const int cAlign = 16;
         int cNew = !cAlloc ? cSize : cSize + (cAlign-1) - ((cSize) % cAlign);
         T* p = new T[cNew];
         if ( ! p) return false;

         // if there is an existing buffer copy items from it to the new buffer
         int cCopy = 0;
         if (pbuf) {
            cCopy = cItems;
            for (int ix = 0; ix > 0 - cCopy; --ix)
               p[(ix+cCopy)%cSize] = (*this)[ix];
            delete[] pbuf;
         }

         pbuf    = p;
         cAlloc  = cNew;
         cMax    = cSize;
         ixHead  = cCopy;
         cItems  = cCopy;
   
      } else if (cSize < cMax) {

         // because of the mustcopy test above, we should only
         // get here if there if cItems is 0 or the current items
         // all fit within the new range from 0 to cSize
         // we shouldn't need to correct ixHead or cItems, but
         // just to be careful, fix them up anyway.
         if (cItems > 0) {
            ixHead = (ixHead + cSize) % cSize;
            if (cItems > cSize) 
               cItems = cSize;
         }
      }
      cMax = cSize;
      return true;
   }

   int Unexpected() {
     #ifdef EXCEPT
      EXCEPT("Unexpected call to empty ring_buffer\n");
     #endif
      return 0;
   }


   // push a new latest item, returns the item that was discarded
   T Push(T val) {
      if (cItems > cMax) return T(Unexpected());
      if ( ! pbuf) SetSize(2);

      // advance the head item pointer
      ixHead = (++ixHead) % cMax;

      // if we have room to add an item without overwriting one
      // then also grow the counter.
      if (cItems < cMax) {
         pbuf[ixHead] = val;
         ++cItems;
         return 0;
      }

      // we get here if cItems == cMax. 
      // we can't add an item without removing one
      // so save off the one we will remove and return it.
      T tmp = pbuf[ixHead];
      pbuf[ixHead] = val;
      return tmp;
   }

   // remove the head item and return it.
   T Pop() {
      if (cItems > 0) {
         T tmp = pbuf[ixHead];
         --cItems;
         --ixHead;
         if (0 == cItems) 
            ixHead = 0;
         return tmp;
      }
      return 0;
   }

   // add to the head item.
   T Add(T val) {
      if ( ! pbuf || ! cMax) return T(Unexpected());
      pbuf[ixHead] += val;
      return pbuf[ixHead];
   }

   // advance to the head item to next slot in the ring buffer,
   // this is equivalent to Push(0) except that it does NOT allocate
   // a ring buffer if there isn't one.
   //
   T Advance() { 
      if (empty()) return 0;
      return Push(T(0)); 
   }
};

// templatize publishing a value to ClassAd's so that we can specialize on types
// that ClassAd's don't support and do the right thing.
//
template <class T>
inline int ClassAdAssign(ClassAd & ad, const char * pattr, T value) {
   return ad.Assign(pattr, value);
}
template <>
inline int ClassAdAssign(ClassAd & ad, const char * pattr, int64_t value) {
   return ad.Assign(pattr, (int)value);
}

template <class T>
inline int ClassAdAssign2(ClassAd & ad, const char * pattr1, const char * pattr2, T value) {
   MyString attr(pattr1);
   attr += pattr2;
   return ad.Assign(attr.Value(), value);
}
template <>
inline int ClassAdAssign2(ClassAd & ad, const char * pattr1, const char * pattr2, int64_t value) {
   return ClassAdAssign2(ad, pattr1, pattr2, (int)value);
}

// base class for all statistics probes
//
class stats_entry_base;
typedef void (stats_entry_base::*FN_STATS_ENTRY_PUBLISH)(ClassAd & ad, const char * pattr, int flags) const;
typedef void (stats_entry_base::*FN_STATS_ENTRY_ADVANCE)(int cAdvance);
typedef void (stats_entry_base::*FN_STATS_ENTRY_SETRECENTMAX)(int cRecent);
typedef void (stats_entry_base::*FN_STATS_ENTRY_CLEAR)(void);
typedef void (*FN_STATS_ENTRY_DELETE)(void* probe);

class stats_entry_base {
public:
   static const int unit = 0;
   //void AdvanceBy(int cSlots) {};
   //void SetRecentMax(int cRecentMax) {};
   static FN_STATS_ENTRY_ADVANCE GetFnAdvance() { return NULL; };
   static FN_STATS_ENTRY_SETRECENTMAX GetFnSetRecentMax() { return NULL; };
};


// stats_entry_count holds a single value, that can only count up,
// it is the simplist of all possible statistics values because
//
template <class T> class stats_entry_count : public stats_entry_base {
public:
   stats_entry_count() : value(0) {}
   T value;
   void Publish(ClassAd & ad, const char * pattr, int flags) const { 
      ClassAdAssign(ad, pattr, value); 
      };

   static const int unit = IS_CLS_COUNT | stats_entry_type<T>::id;
   static void PublishValue(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_count<T> * pthis = (const stats_entry_count<T> *)me;
      ClassAdAssign(ad, pattr, pthis->value);
      }
};

// use stats_entry_abs for entries that have an absolute value such as Number of jobs currently running.
// this entry keeps track of the largest value as the value changes.
//
template <class T> class stats_entry_abs : public stats_entry_count<T> {
public:
   stats_entry_abs() : largest(0) {}
   T largest;

   static const int PubValue = 1;
   static const int PubLargest = 2;
   static const int PubDecorateAttr = 0x100;
   static const int PubDefault = PubValue | PubLargest | PubDecorateAttr;
   void Publish(ClassAd & ad, const char * pattr, int flags) const { 
      if ( ! flags) flags = PubDefault;
      if (flags & this->PubValue)
         ClassAdAssign(ad, pattr, this->value); 
      if (flags & this->PubLargest) {
         if (flags & this->PubDecorateAttr)
            ClassAdAssign2(ad, pattr, "Peak", largest);
         else
            ClassAdAssign(ad, pattr, largest); 
      }
   }

   void Clear() {
      this->value = 0;
      largest = 0;
   }

   T Set(T val) { 
      if (val > largest)
         largest = val;
      this->value = val;
      return this->value;
   }

   T Add(T val) { return Set(this->value + val); }

   // operator overloads
   stats_entry_abs<T>& operator=(T val)  { Set(val); return *this; }
   stats_entry_abs<T>& operator+=(T val) { Add(val); return *this; }

   // these enable publishing using a static const table of GenericStatsPub entries.
   //
   static const int unit = IS_CLS_ABS | stats_entry_type<T>::id;
   static void PublishLargest(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_abs<T> * pthis = (const stats_entry_abs<T> *)me;
      ClassAdAssign(ad, pattr, pthis->largest);
      }
};

// use stats_entry_recent for values that are constantly increasing, such 
// as number of jobs run.  this class keeps track of a the recent total
// as well as the overall total - new values are added to recent and old
// values are subtracted from it. 
//
template <class T> class stats_entry_recent : public stats_entry_count<T> {
public:
   stats_entry_recent(int cRecentMax=0) : recent(0), buf(cRecentMax) {}
   T recent;            // the up-to-date recent value (for publishing)
   ring_buffer<T> buf;  // use to store a buffer of older values

   static const int PubValue = 1;
   static const int PubRecent = 2;
   static const int PubDebug = 0x80;
   static const int PubDecorateAttr = 0x100;
   static const int PubValueAndRecent = PubValue | PubRecent | PubDecorateAttr;
   static const int PubDefault = PubValueAndRecent;
   void Publish(ClassAd & ad, const char * pattr, int flags) const { 
      if ( ! flags) flags = PubDefault;
      if (flags & this->PubValue)
         ClassAdAssign(ad, pattr, this->value); 
      if (flags & this->PubRecent) {
         if (flags & this->PubDecorateAttr)
            ClassAdAssign2(ad, "Recent", pattr, recent);
         else
            ClassAdAssign(ad, pattr, recent); 
      }
      if (flags & this->PubDebug) {
         PublishDebug(ad, pattr, flags);
      }
   }

   void PublishDebug(ClassAd & ad, const char * pattr, int flags) const;

   void Clear() {
      this->value = 0;
      recent = 0;
      buf.Clear();
   }
   void ClearRecent() {
      recent = 0;
      buf.Clear();
   }

   T Add(T val) { 
      this->value += val; 
      recent += val;
      if (buf.MaxSize() > 0) {
         if (buf.empty())
            buf.Push(val);
         else
            buf.Add(val);
      }
      return this->value; 
   }

   // Advance to the next time slot and add a value.
   T Advance(T val) { 
      this->value += val; 
      if (buf.MaxSize() > 0) {
         recent -= buf.Push(val);
         recent += val;
      } else {
         recent = val;
      }
      return this->value; 
   }

   // Advance by cSlots time slots
   void AdvanceBy(int cSlots) { 
      if (cSlots < buf.MaxSize()) {
         while (cSlots > 0) {
            recent -= buf.Advance();
            --cSlots;
         }
      } else {
         recent = 0;
         buf.Clear();
      }
   }

   T Set(T val) { 
      T delta = val - this->value;
      return Add(delta);
   }

   void SetWindowSize(int size) {
      buf.SetSize(size);
   }
   void SetRecentMax(int cRecentMax) {
      buf.SetSize(cRecentMax);
   }

   // operator overloads
   stats_entry_recent<T>& operator=(T val)  { Set(val); return *this; }
   stats_entry_recent<T>& operator+=(T val) { Add(val); return *this; }

   // these enable publishing using a static const table of GenericStatsPub entries.
   //
   static const int unit = IS_RECENT | stats_entry_type<T>::id;
   static void PublishRecent(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_recent<T> * pthis = (const stats_entry_recent<T> *)me;
      ClassAdAssign(ad, pattr, pthis->recent);
      }
   static FN_STATS_ENTRY_ADVANCE GetFnAdvance() { return (FN_STATS_ENTRY_ADVANCE)&stats_entry_recent<T>::AdvanceBy; };
   static FN_STATS_ENTRY_SETRECENTMAX GetFnSetRecentMax() { return (FN_STATS_ENTRY_SETRECENTMAX)&stats_entry_recent<T>::SetRecentMax; };
   static void Delete(stats_entry_recent<T> * probe) { delete probe; }
};

// use timed_queue to keep track of recent windowed values.
// obsolete: use stats_entry_tq for windowed values that need more time accuracy than
// can be provided by the ring_buffer
//
#ifdef _timed_queue_h_

template <class T> class stats_entry_tq : public stats_entry_count<T> {
public:
   stats_entry_tq() : recent(0) {}
   T recent;
   timed_queue<T> tq;

   static const int PubValue = 1;
   static const int PubRecent = 2;
   static const int PubDebug = 4;
   static const int PubDecorateAttr = 0x100;
   static const int PubValueAndRecent = PubValue | PubRecent | PubDecorateAttr;
   static const int PubDefault = PubValueAndRecent;
   void Publish(ClassAd & ad, const char * pattr, int flags) const { 
      if ( ! flags) flags = PubDefault;
      if (flags & this->PubValue)
         ClassAdAssign(ad, pattr, this->value); 
      if (flags & this->PubRecent) {
         if (flags & this->PubDecorateAttr)
            ClassAdAssign2(ad, "Recent", pattr, recent);
         else
            ClassAdAssign(ad, pattr, recent); 
      }
   }

   void Clear() {
      this->value = 0;
      recent = 0;
      tq.clear();
   }
   void ClearRecent() {
      recent = 0;
      tq.clear();
   }

   T Add(T val, time_t now) { 
      this->value += val; 
      if (val != 0) {
         if (tq.empty() || tq.front().first != now) 
            tq.push_front(val, now);
         else
            tq.front().second += val;
      }
      return this->value; 
   }

   T Set(T val, time_t now) { 
      T delta = val - this->value;
      return Add(delta, now);
   }

   T Accumulate(time_t now, time_t window) {
      tq.trim_time(now - window);
      T ret(0);
      for (typename timed_queue<T>::iterator jj(tq.begin());  jj != tq.end();  ++jj) {
         ret += jj->second;
         }
      recent = ret;
      return ret;
   }

   // the the max size of the 
   void SetMaxTime(int size) { tq.max_time(size); }

   // these enable publishing using a static const table of GenericStatsPub entries.
   //
   static const int unit = IS_RECENTTQ | stats_entry_type<T>::id;
   static void PublishRecent(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_tq<T> * pthis = (const stats_entry_tq<T> *)me;
      ClassAdAssign(ad, pattr, pthis->recent);
      }
   static void Delete(stats_entry_tq<T> * probe) { delete probe; }
};

// specialize for time_t because ClassAd doesn't handle 64 bit ints
// template <> void stats_entry_tq<time_t>::PublishRecent(const char * me, ClassAd & ad, const char * pattr);

#endif // _timed_queue_h_

#undef min
#undef max
#include <limits>

// stats_entry_probe is derived from Miron Livny's Probe class,
// it counts and sums samples as they arrive and can publish
// Min,Max,Average,Variance and Standard Deviation for the data set.
//
// NOTE: the probe derives from the simple counter template and uses
// its 'value' field to hold the count of samples.  the value of the
// samples themselves are not stored, only the sum, min and max are stored.
//
template <class T> class stats_entry_probe : protected stats_entry_count<T> {
public:
   stats_entry_probe() 
      : Max(std::numeric_limits<T>::min())
      , Min(std::numeric_limits<T>::max())
      , Sum(0)
      , SumSq(0) 
   {
   }

protected:
   T Max;        // max sample so far
   T Min;        // min sample so far
   T Sum;        // Sum of samples
   T SumSq;      // Sum of samples squared

public:
   void Publish(ClassAd & ad, const char * pattr, int flags) const;

   void Clear() {
      this->value = 0; // value is use to store the count of samples.
      Max = std::numeric_limits<T>::min();
      Min = std::numeric_limits<T>::max();
      Sum = 0;
      SumSq = 0;
   }

   T Add(T val) { 
      this->value += 1; // value is use to store the count of samples.
      if (val > Max) Max = val;
      if (val < Min) Min = val;
      Sum += val;
      SumSq += val*val;
      return Sum;
   }

   T Count() { return this->value; }

   T Avg() {
      if (Count() > 0) {
         return this->Sum / Count();
      } else {
         return this->Sum;
      }
   }

   T Var() {
      if (Count() <= 1) {
         return this->Min;
      } else {
         // Var == (SumSQ - count*Avg*Avg)/(count -1)
         return (SumSq - this->Sum * (this->Sum / Count()))/(Count() - 1);
      }
   }

   T Std() {
      if (Count() <= 1) {
         return this->Min;
      } else {
         return sqrt(Var());
      }
   }

   // operator overloads
   stats_entry_probe<T>& operator+=(T val) { Add(val); return *this; }

   // these enable publishing using a static const table of GenericStatsPub entries.
   //
   static const int unit = IS_CLS_PROBE | stats_entry_type<T>::id;
   static void PublishLargest(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_probe<T> * pthis = (const stats_entry_probe<T> *)me;
      ClassAdAssign(ad, pattr, pthis->Max);
      }
   static void PublishAverage(const char * me, ClassAd & ad, const char * pattr) {
      const stats_entry_probe<T> * pthis = (const stats_entry_probe<T> *)me;
      ClassAdAssign(ad, pattr, pthis->Avg());
      }
   static void Delete(stats_entry_probe<T> * probe) { delete probe; }
};

// A statistics probe designed to keep track of accumulated running time
// of a data set.  keeps a count of times that time was added and
// a running total of time
//
class stats_recent_counter_timer : public stats_entry_base {
private:
   stats_entry_recent<int> count;
   stats_entry_recent<double> runtime;

public:
   stats_recent_counter_timer(int cRecentMax=0) 
      : count(cRecentMax)
      , runtime(cRecentMax) 
      {
      };

   double Add(double sec)     { count += 1; runtime += sec; return runtime.value; }
   time_t Add(time_t time)    { count += 1; runtime += double(time); return (time_t)runtime.value; }
   void Clear()              { count.Clear(); runtime.Clear();}
   void ClearRecent()        { count.ClearRecent(); runtime.ClearRecent(); }
   void AdvanceBy(int cSlots) { count.AdvanceBy(cSlots); runtime.AdvanceBy(cSlots); }
   void SetRecentMax(int cMax)    { count.SetRecentMax(cMax); runtime.SetRecentMax(cMax); }
   double operator+=(double val)    { return Add(val); }

   void Publish(ClassAd & ad, const char * pattr, int flags) const;

   static const int unit = IS_RCT | stats_entry_type<int>::id;
   static void PublishValue(const char * me, ClassAd & ad, const char * pattr);
   static void Delete(stats_recent_counter_timer * pthis);
};


/* ----------------------------------------------------------------------------- 
 * helper functions for dealing with collections of statistics 
 * these functions expect to be passed and array of GenericStatsPubItem
 * with one entry for each statistics value that you wish to publish and or manipulate
 * and a pointer to the base address of the statistics data. 
 */

// set the ring buffer size for IS_RECENT type stats. and time window size for IS_TIMED_QUEUE stats
/*
void generic_stats_SetRecentMax(
   const GenericStatsPubItem * pTable, 
   int    cTable, 
   char * pstruct, 
   int    window, 
   int    quantum);

// publish items into ClassAds using the static Publish methods
void generic_stats_PublishToClassAd(ClassAd & ad, const GenericStatsPubItem * pTable, int cTable, const char * pdata);
void generic_stats_DeleteInClassAd(ClassAd & ad, const GenericStatsPubItem * pTable, int cTable, const char * pdata);

// reset all counters, and flush the Recent buffers.
void generic_stats_Clear(const GenericStatsPubItem * pTable, int cTable, char * pdata);
// flush the Recent buffers
void generic_stats_ClearRecent(const GenericStatsPubItem * pTable, int cTable, char * pdata);
// advance the Recent buffers to the next slot
void generic_stats_AdvanceRecent(const GenericStatsPubItem * pTable, int cTable, char * pdata, int cAdvance);

// determine of enough time has passed to advance the Recent buffers and if so, Advance
// this function will modify LastUpdateTime, Lifetime and may modify RecentTickTime and
// RecentLifetime.
// returns number of Quantums that stats were/should be Advanced
//
int generic_stats_Tick(
   const GenericStatsPubItem * pPub, 
   int    cPub, 
   char * pdata,
   int    RecentMaxTime,
   int    RecentQuantum,
   time_t InitTime,
   time_t & LastUpdateTime,  // in,out
   time_t & RecentTickTime,  // in,out
   time_t & Lifetime,        // in,out
   time_t & RecentLifetime); // in,out
*/

int generic_stats_Tick(
   int    RecentMaxTime,
   int    RecentQuantum,
   time_t InitTime,
   time_t & LastUpdateTime,  // in,out
   time_t & RecentTickTime,  // in,out
   time_t & Lifetime,        // in,out
   time_t & RecentLifetime); // in,out

#ifndef FIELDOFF
 #ifdef WIN32
  #define FIELDOFF(st,fld) FIELD_OFFSET(st, fld)
 #else
  //#define FIELDOFF(st,fld) ((int)(size_t)&(((st *)0)->fld))
  #define FIELDOFF(st,fld) offsetof(st,fld)
 #endif
 #define FIELDSIZ(st,fld) ((int)(sizeof(((st *)0)->fld)))
#endif

// use these to help initialize arrays of GenericStatsPubItem's
//
#define GS_FIELD(st,fld) (((st *)0)->fld)

#define GENERIC_STATS_PUB_TYPE(st,pre,name,as,T) { pre #name, as | stats_entry_type<T>::id, FIELDOFF(st,name), &stats_entry_count<T>::PublishValue }
#define GENERIC_STATS_PUB(st,pre,name,as)        { pre #name, as | GS_FIELD(st,name).unit, FIELDOFF(st,name), &GS_FIELD(st,name).PublishValue }
#define GENERIC_STATS_PUB_PEAK(st,pre,name,as)   { pre #name "Peak", as | GS_FIELD(st,name).unit, FIELDOFF(st,name), &GS_FIELD(st,name).PublishLargest }
#define GENERIC_STATS_PUB_AVG(st,pre,name,as)    { pre #name "Avg", as | GS_FIELD(st,name).unit, FIELDOFF(st,name), &GS_FIELD(st,name).PublishAverage }
#define GENERIC_STATS_PUB_RECENT(st,pre,name,as) { "Recent" pre #name, as | GS_FIELD(st,name).unit, FIELDOFF(st,name), &GS_FIELD(st,name).PublishRecent }
#define GENERIC_STATS_PUB_RECENT_DEBUG(st,pre,name,as) { "Recent" pre #name "Debug", as | GS_FIELD(st,name).unit, FIELDOFF(st,name), &GS_FIELD(st,name).PublishDebug }

// the stats_pool class is used to hold a collection of statistics probes of various types
// probes in the pool can be Cleared, Advanced and Published as a group. 
//
// Probes may be created by the pool, in which case they will be deleted when the pool is 
// destroyed.  Or they can be created externally and added to the pool, in which case the
// creator is responsible for destroying them.  This allows probes to be defined as member
// variables to a class, then added to the pool to get the benefit of pool Publish, Advance
// and Clear methods.
//

class stats_pool {
public:
   stats_pool(int size=30) 
      : pub(size, MyStringHash, updateDuplicateKeys) 
      , pool(size, hashFuncVoidPtr, updateDuplicateKeys) 
      {
      };

   // allocate a probe and insert it into the pool.
   //
   template <typename T> T* NewProbe(
      const char * name,       // unique name for the probe
      const char * pattr=NULL, // publish attribute name
      int          flags=0)    // flags to control publishing
   {
      T* probe = GetProbe<T>(name);
      if (probe)
         return probe;

      probe = new T();
      bool fOwnedByPool = true;
      InsertProbe(name, T::unit, (void*)probe, 
                  fOwnedByPool,
                  pattr ? strdup(pattr) : NULL, 
                  flags,
                  (FN_STATS_ENTRY_PUBLISH)&T::Publish,
                  T::GetFnAdvance(), //(FN_STATS_ENTRY_ADVANCE)&T::AdvanceBy, 
                  (FN_STATS_ENTRY_CLEAR)&T::Clear,
                  T::GetFnSetRecentMax(), //(FN_STATS_ENTRY_SETRECENTMAX)&T::SetRecentMax,
                  (FN_STATS_ENTRY_DELETE)&T::Delete);
      return probe;
   }

   // lookup a probe by name
   //
   template <typename T> T* GetProbe(const char * name)
   {
      pubitem item;
      if (pub.lookup(name, item) >= 0)
         return (T*)item.pitem;
      return 0;
   }

   // add an externally created probe to the pool
   // so we can use pool functions to Advance/Clear/Publish
   //
   template <typename T> T* AddProbe (
      const char * name,       // unique name for the probe
      T*           probe,      // the probe, usually a member of a class/struct
      const char * pattr=NULL, // publish attribute name
      int          flags=0,    // flags to control publishing
      FN_STATS_ENTRY_PUBLISH fnpub=NULL) // publish method
   {
      T* probeExist = GetProbe<T>(name);
      if (probeExist)
         return probeExist;

      bool fOwnedByPool = false;
      InsertProbe(name, T::unit, (void*)probe, 
                  fOwnedByPool,
                  pattr, 
                  flags,
                  fnpub ? fnpub : (FN_STATS_ENTRY_PUBLISH)&T::Publish,
                  T::GetFnAdvance(), //(FN_STATS_ENTRY_ADVANCE)&T::AdvanceBy, 
                  (FN_STATS_ENTRY_CLEAR)&T::Clear,
                  T::GetFnSetRecentMax(), //(FN_STATS_ENTRY_SETRECENTMAX)&T::SetRecentMax,
                  NULL);
      return probe;
   }

   // add an additional publishing entry for a probe that is already in the pool
   //
   template <typename T> T* AddPublish (
      const char * name,       // unique name for the probe
      T*           probe,      // the probe, usually a member of a class/struct
      const char * pattr,      // unique attr, must not be the same as a probe name.
      int          flags=0,    // flags to control publishing
      FN_STATS_ENTRY_PUBLISH fnpub=NULL) // publish method
   {
      T* probeExist = GetProbe<T>(name);
      if (probeExist)
         return probeExist;

      bool fOwnedByPool = false;
      InsertPublish(name, T::unit, (void*)probe, 
                    fOwnedByPool,
                    pattr, 
                    flags,
                    fnpub ? fnpub : (FN_STATS_ENTRY_PUBLISH)&T::Publish);
      return probe;
   }

   int RemoveProbe (const char * name); // remove from pool, will delete if owned by pool

   double  SetSample(const char * probe_name, double sample);
   int     SetSample(const char * probe_name, int sample);
   int64_t SetSample(const char * probe_name, int64_t sample);

   void Clear();
   void ClearRecent();
   void SetRecentMax(int window, int quantum);
   int  Advance(int cAdvance);
   void Publish(ClassAd & ad) const;
   void Unpublish(ClassAd & ad) const;

private:
   struct pubitem {
      int    units;    // copied from the class->unit, identifies the class and type of probe
      int    flags;    // passed to Publish
      int    fOwnedByPool;
      void * pitem;    // pointer to stats_entry_base derived class instance class/struct
      const char * pattr; // if non-null passed to Publish, if null name is passed.
      FN_STATS_ENTRY_PUBLISH Publish;
   };
   struct poolitem {
      int units;
      int fOwnedByPool; // true if created and owned by this, otherise owned by some other code.
      FN_STATS_ENTRY_ADVANCE Advance;
      FN_STATS_ENTRY_CLEAR   Clear;
      FN_STATS_ENTRY_SETRECENTMAX SetRecentMax;
      FN_STATS_ENTRY_DELETE  Delete;
   };
   // table of values to publish, possibly more than one for each probe
   HashTable<MyString,pubitem> pub;

   // table of unique probes counters, used to Advance and Clear the items.
   HashTable<void*,poolitem> pool;

   void InsertProbe (
      const char * name,       // unique name for the probe
      int          unit,       // identifies the probe class/type
      void*        probe,      // the probe, usually a member of a class/struct
      bool         fOwned,     // probe and pattr string are owned by the pool
      const char * pattr,      // publish attribute name
      int          flags,      // flags to control publishing
      FN_STATS_ENTRY_PUBLISH fnpub, // publish method
      FN_STATS_ENTRY_ADVANCE fnadv, // Advance method
      FN_STATS_ENTRY_CLEAR   fnclr, // Clear method
      FN_STATS_ENTRY_SETRECENTMAX fnsrm,
      FN_STATS_ENTRY_DELETE  fndel); // static Delete method

   void InsertPublish (
      const char * name,       // unique name for the probe
      int          unit,       // identifies the probe class/type
      void*        probe,      // the probe, usually a member of a class/struct
      bool         fOwned,     // probe and pattr string are owned by the pool
      const char * pattr,      // publish attribute name
      int          flags,      // flags to control publishing
      FN_STATS_ENTRY_PUBLISH fnpub); // publish method

};

// the macros help to add statistics probe defined as class or struct members to
// a stats_pool. use STATS_POOL_ADD or STATS_POOL_ADD_VAL to add a probe to the pool
// then use STATS_POOL_PUB_xxx to add additional Publish entries as needed.
#define STATS_POOL_ADD(pool,pre,name,as)        (pool).AddProbe(#name, &name, pre #name, name.PubDefault)
#define STATS_POOL_ADD_VAL(pool,pre,name,as)    (pool).AddProbe(#name, &name, pre #name, name.PubValue)
#define STATS_POOL_PUB_PEAK(pool,pre,name,as)   (pool).AddPublish(#name "Peak", &name, pre #name "Peak", name.PubLargest)
#define STATS_POOL_PUB_RECENT(pool,pre,name,as) (pool).AddPublish("Recent" #name, &name, "Recent" pre #name, name.PubRecent)
#define STATS_POOL_PUB_DEBUG(pool,pre,name,as)  (pool).AddPublish(#name "Debug", &name, pre #name "Debug", name.PubDebug)



#endif /* _GENERIC_STATS_H */
