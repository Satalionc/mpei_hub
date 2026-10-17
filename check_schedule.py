#!/usr/bin/env python3
"""Проверка расписания МЭИ без C++ — работает сразу."""
import json, sys, urllib.parse, urllib.request
from datetime import date, timedelta
from collections import OrderedDict

def get(url):
    req = urllib.request.Request(url, headers={"Accept": "application/json", "User-Agent": "mpei-check/1"})
    with urllib.request.urlopen(req, timeout=20) as r:
        return json.loads(r.read().decode())

group = sys.argv[1] if len(sys.argv) > 1 else "ЭРэ-41-26"
# текущая неделя (пн-вс) относительно сегодня
today = date.today()
monday = today - timedelta(days=(today.weekday()))  # Mon=0
sunday = monday + timedelta(days=6)

print(f"Группа: {group}")
print(f"Неделя: {monday} — {sunday}")

search = get("https://ts.mpei.ru/api/search?term=" + urllib.parse.quote(group))
groups = [x for x in search if x.get("type") == "group" and x.get("label") == group]
if not groups:
    groups = [x for x in search if x.get("type") == "group"]
    print("Точного совпадения нет, кандидаты:")
    for g in groups[:10]:
        print(" ", g)
    if not groups:
        sys.exit(1)
    print("Беру первого:", groups[0]["label"])

oid = groups[0]["id"]
print(f"oid = {oid}")

start = monday.strftime("%Y.%m.%d")
finish = sunday.strftime("%Y.%m.%d")
url = f"https://ts.mpei.ru/api/schedule/group/{oid}?start={start}&finish={finish}"
print("URL:", url)
lessons = get(url)
print(f"Занятий: {len(lessons)}\n")

by = OrderedDict(( (monday + timedelta(days=i)).strftime("%Y.%m.%d"), [] ) for i in range(7))
for L in lessons:
    d = L.get("date")
    if d in by:
        by[d].append(L)

wd = "Пн Вт Ср Чт Пт Сб Вс".split()
for i, (dk, items) in enumerate(by.items()):
    items.sort(key=lambda x: x.get("beginLesson", ""))
    print(f"{wd[i]} {dk}: {len(items)} пар")
    for L in items:
        print(f"  {L.get('beginLesson')}-{L.get('endLesson')}  {L.get('kindOfWork')}  {L.get('discipline')}  [{L.get('auditorium')}]")
    if not items:
        print("  (нет)")
