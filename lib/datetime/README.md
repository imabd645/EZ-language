# datetime

Dates, times and time zones for EZ. Timestamps are milliseconds since
`1970-01-01T00:00:00 UTC`.

```ez
use "datetime"

now()                                        # 1786538985123
toIso(now())                                 # "2026-08-12T12:49:45.123Z"
parse("2026-08-12T14:30:00+05:30")           # an instant, offset applied
formatIn(now(), "Asia/Kolkata", "HH:mm ZZ")  # "18:19 +05:30"
relativeTime(then, now())                    # "3 hours ago"
```

One import. Everything is exported from the package root — the submodules are
how the code is organised, not how it is used.

## Install

```
ez install datetime
```

No dependencies.

## What's new in 1.1

Everything from 1.0 still works and still means the same thing; the outputs
were diffed against the published 1.0 across the epoch, a leap day, Y2K and the
2038 boundary, and are identical.

- **Real time zones, with DST.** 1.0 offered a manual whole-hour offset, which
  cannot express India (+05:30) or Nepal (+05:45) at all and has no idea a
  zone's offset changes twice a year.
- **Parsing.** 1.0 could not read a timestamp back at all.
- **Pattern formatting** beyond the single fixed layout.
- **Calendar arithmetic** — months, weeks, business days, quarters, ISO weeks.
- **Relative time and durations.**
- **Dates before 1970 now work.** See below.

### The pre-1970 fix

1.0 converted a timestamp by counting forward from 1970 a year at a time. With
a negative timestamp neither loop ran:

```ez
format(-1)      # 1.0: "1970-01-00 23:59:59"      1.1: "1969-12-31 23:59:59"
```

The cause was `floorDiv`, which truncated towards zero rather than flooring and
so disagreed with its own name for negative inputs. It now floors.
`floorDiv(-1, 1000)` gives `-1` where it used to give `0`; results for
non-negative inputs are unchanged.

Conversion is now closed-form integer arithmetic rather than a loop, so it is
also O(1) instead of O(years).

## Time zones

```ez
offsetIn(ts, "Europe/London")                # minutes east of Greenwich
formatIn(ts, "America/New_York", "YYYY-MM-DD HH:mm")
isoIn(ts, "Asia/Tokyo")                      # "2026-07-16T07:00:00.000+09:00"
describeIn(ts, "Asia/Kolkata")               # "… +05:30 (India Standard Time)"
componentsIn(ts, zone)                       # parts + offsetMinutes + isDst
nowIn(zone)
isDst(ts, zone)      hasDst(zone)      localZone()      listZones()
```

A zone's offset is **not a property of the zone** — it is a property of the
instant:

```ez
offsetIn(january, "Europe/London")           # 0
offsetIn(july,    "Europe/London")           # 60
offsetIn(january, "Australia/Sydney")        # 660   (southern summer)
offsetIn(july,    "Australia/Sydney")        # 600
```

This uses the zone database Windows already ships and keeps updated, so the
rules are the real ones — including historical changes, so a date in 2019 uses
the rules that were in force in 2019. Nothing is downloaded and no tzdata file
is bundled. 141 zones on a current Windows 11.

### Naming

IANA names (`Asia/Kolkata`), Windows names (`India Standard Time`), `UTC`, and
fixed offsets (`+05:30`, `-0800`) all work, case-insensitively. An unknown zone
raises a clear error rather than silently using the wrong offset.

A fixed offset is deliberately *not* treated as a zone: it has no DST rules, so
`+01:00` means the same thing in January and July.

### Going the other way

```ez
instantIn("Europe/Paris", 2026, 7, 15, 9, 0, 0)   # 09:00 Paris, as an instant
parseIn("2026-07-15 17:30:00", "Asia/Kolkata")    # a typed local time
```

Two hours a year are genuinely ambiguous or missing — when the clocks go back
01:30 happens twice, and when they go forward it never happens at all. Windows
resolves both; which way is not something to depend on. Schedule in UTC if it
matters.

## Parsing

```ez
parse("2026-08-12")
parse("2026-08-12T14:30:00Z")
parse("2026-08-12 14:30:00+05:30")           # the offset is applied
tryParse(text)                               # nil instead of a throw
isValidDate(text)
```

Handles ISO 8601 as it actually arrives: `T` or a space, with or without
milliseconds, offsets with or without a colon, a comma as the decimal
separator, Postgres microseconds (truncated to milliseconds), `24:00:00` as the
end of a day, and leap seconds (clamped). Refuses `2026-02-30` and
`2026-02-29`, and says why.

## Formatting

```ez
toIso(ts)          toIsoSeconds(ts)     toIsoDate(ts)     toHttpDate(ts)
formatWith(ts, "dddd D MMMM YYYY")     # "Wednesday 12 August 2026"
```

| | | | | | |
|---|---|---|---|---|---|
| `YYYY` 2026 | `MM` 08 | `DD` 12 | `HH` 14 | `mm` 05 | `SSS` 123 |
| `YY` 26 | `M` 8 | `D` 12 | `hh` 02 | `ss` 09 | `A` PM |
| `MMMM` August | `MMM` Aug | `dddd` Wednesday | `ddd` Wed | `a` pm | `ZZ` +05:30 |

Longest token wins, so `YYYY` is never read as `YY` twice. `\` escapes a letter
meant literally.

## Calendar arithmetic

```ez
addDays / addWeeks / addHours / addMinutes / addSeconds
addMonths(ts, n)     addYears(ts, n)
startOfDay / startOfWeek / startOfMonth / startOfYear
endOfDay   / endOfWeek   / endOfMonth   / endOfYear
```

Months are not a fixed number of milliseconds, so `addMonths` moves the month
field and clamps the day:

```ez
addMonths(jan31, 1)      # 28 February — not 3 March
addYears(feb29, 1)       # 28 February
```

## Comparing

```ez
isSameDay / isSameMonth / isSameYear / isBefore / isAfter / isBetween
isPast / isFuture / isToday / isWeekend / isWeekday
earliest(list)     latest(list)
daysBetween(a, b)              # whole calendar days, ignoring the time
businessDaysBetween(a, b)      # excluding weekends
addBusinessDays(ts, n)
age(birth, at)                 # decremented if the birthday has not happened
weekOf(ts)                     # ISO week: { year, week }
quarterOf(ts)
```

`daysBetween` counts whole calendar days, which is what "how many days until
Friday" means — not the millisecond difference divided by a day.

`weekOf` follows ISO 8601: the week containing the first Thursday is week 1, so
1 January 2027 is week 53 **of 2026**.

## Durations

```ez
humanDuration(93784000)          # "1d 2h"
relativeTime(ts, from)           # "3 hours ago", "in 2 days"
breakDuration(ms)                # { days, hours, minutes, seconds, … }
timeIt(|| doWork())              # { result, ms }
```

## Tests

```
ez test.ez
```

207 assertions. The anchors can all be checked by hand — the epoch, Y2K, a leap
day, the 2038 overflow — because everything else depends on the calendar being
right. The zone section tests January *and* July for the same zone, in both
hemispheres.

## License

MIT
