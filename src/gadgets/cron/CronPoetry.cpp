#include "gadgets/cron/CronPoetry.hpp"
#include "cron/config/CronConfiguration.hpp"

namespace batbox::gadgets::cron {
std::string buildCronCreateDescription(bool durableEnabled){
  return durableEnabled?
    "Schedule a prompt to run at a future time — either recurring on a cron schedule, or once at a specific time. Pass durable: true to persist to .batbox/scheduled_tasks.json; otherwise session-only.":
    "Schedule a prompt to run at a future time within this Batbox session — either recurring on a cron schedule, or once at a specific time.";
}
std::string buildCronCreatePoetry(bool durableEnabled){
  const std::string durabilitySection=durableEnabled?
    R"CRON(## Durability

By default (durable: false) the job lives only in this Batbox session — nothing is written to disk, and the job is gone when Batbox exits. Pass durable: true to write to .batbox/scheduled_tasks.json so the job survives restarts. Only use durable: true when the user explicitly asks for the task to persist ("keep doing this every day", "set this up permanently"). Most "remind me in 5 minutes" / "check back in an hour" requests should stay session-only.)CRON":
    R"CRON(## Session-only

Jobs live only in this Batbox session — nothing is written to disk, and the job is gone when Batbox exits.)CRON";
  const std::string runtimeNote=durableEnabled?
    "Durable jobs persist to .batbox/scheduled_tasks.json and survive session restarts — on next launch they resume automatically. One-shot durable tasks that were missed while the REPL was closed are surfaced for catch-up. Session-only jobs die with the process. ":"";
  const auto age=std::to_string(static_cast<int>(batbox::cron::config::maxAgeDays));
  std::string result=R"CRON(Schedule a prompt to be enqueued at a future time. Use for both recurring schedules and one-shot reminders.

Uses standard 5-field cron in the user's local timezone: minute hour day-of-month month day-of-week. "0 9 * * *" means 9am local — no timezone conversion needed.

## One-shot tasks (recurring: false)

For "remind me at X" or "at <time>, do Y" requests — fire once then auto-delete.
Pin minute/hour/day-of-month/month to specific values:
  "remind me at 2:30pm today to check the deploy" → cron: "30 14 <today_dom> <today_month> *", recurring: false
  "tomorrow morning, run the smoke test" → cron: "57 8 <tomorrow_dom> <tomorrow_month> *", recurring: false

## Recurring jobs (recurring: true, the default)

For "every N minutes" / "every hour" / "weekdays at 9am" requests:
  "*/5 * * * *" (every 5 min), "0 * * * *" (hourly), "0 9 * * 1-5" (weekdays at 9am local)

## Avoid the :00 and :30 minute marks when the task allows it

Every user who asks for "9am" gets `0 9`, and every user who asks for "hourly" gets `0 *` — which means requests from across the planet land on the API at the same instant. When the user's request is approximate, pick a minute that is NOT 0 or 30:
  "every morning around 9" → "57 8 * * *" or "3 9 * * *" (not "0 9 * * *")
  "hourly" → "7 * * * *" (not "0 * * * *")
  "in an hour or so, remind me to..." → pick whatever minute you land on, don't round

Only use minute 0 or 30 when the user names that exact time and clearly means it ("at 9:00 sharp", "at half past", coordinating with a meeting). When in doubt, nudge a few minutes early or late — the user will not notice, and the fleet will.

)CRON";
  result+=durabilitySection;
  result+=R"CRON(

## Runtime behavior

Jobs only fire while the REPL is idle (not mid-query). )CRON";
  result+=runtimeNote;
  result+=R"CRON(The scheduler adds a small deterministic jitter on top of whatever you pick: recurring tasks fire up to 10% of their period late (max 15 min); one-shot tasks landing on :00 or :30 fire up to 90 s early. Picking an off-minute is still the bigger lever.

Recurring tasks auto-expire after )CRON";
  result+=age;
  result+=R"CRON( days — they fire one final time, then are deleted. This bounds session lifetime. Tell the user about the )CRON";
  result+=age;
  result+=R"CRON(-day limit when scheduling recurring jobs.

Returns a job ID you can pass to CronDelete.)CRON";
  return result;
}
std::string buildCronDeletePoetry(bool durableEnabled){
  return durableEnabled?
    "Cancel a cron job previously scheduled with CronCreate. Removes it from .batbox/scheduled_tasks.json (durable jobs) or the in-memory session store (session-only jobs).":
    "Cancel a cron job previously scheduled with CronCreate. Removes it from the in-memory session store.";
}
std::string buildCronListPoetry(bool durableEnabled){
  return durableEnabled?
    "List all cron jobs scheduled via CronCreate, both durable (.batbox/scheduled_tasks.json) and session-only.":
    "List all cron jobs scheduled via CronCreate in this session.";
}
}
