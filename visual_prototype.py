"""
随机合成器 · 视觉原型
------------------------------------------------------------
规则:
  - 球和墙结点都只持有一种性质: PITCH / ADSR / EFFECT
  - 球撞墙: 与墙结点交换性质(永久)
  - 球撞球: 只触发辐射, 不改种类
  - 碰撞点留下衰减场, 效果球额外发出水波, 水波会反弹
  - 球在场内被临时影响, 离开场恢复

操作:
  SPACE 暂停 / R 重置 / N 加 5 个球 / 左键 加球 / 右键 加效果球
  1 圆形  2 矩形  3 椭圆  4 六边形  C 清空场
"""

import math
import random
import sys
import pygame
from pygame.math import Vector2

if "--test" in sys.argv:
    import os
    os.environ["SDL_VIDEODRIVER"] = "dummy"

W, H = 1580, 1000
PANEL_H = 230
CANVAS = pygame.Rect(24, 92, W - 48, H - PANEL_H - 104)


class Settings:
    field_scale = 1.0
    ball_scale = 0.55
    ball_speed = 1.0
    speed_share = 1.0
    speed_strength = 1.0
    speed_cap = 280.0
    wave_speed = 0.65
    wave_range = 0.50
    ratio_pitch = 0.50
    ratio_adsr = 0.25
    ratio_effect = 0.25


SETTINGS = Settings()

BG = (10, 12, 16)
PANEL = (16, 20, 27)
GRID = (26, 31, 40)
WALL = (52, 61, 76)
TEXT = (232, 233, 236)
DIM = (137, 146, 161)
ACCENT = (255, 162, 58)

PITCH = "pitch"
ADSR = "adsr"
EFFECT = "effect"

KIND_COLOR = {
    PITCH: (255, 176, 64),
    ADSR: (76, 214, 199),
    EFFECT: (207, 108, 255),
}
KIND_LABEL = {PITCH: "PITCH", ADSR: "ADSR", EFFECT: "REVERB"}
KIND_RADIUS = {PITCH: 130, ADSR: 105, EFFECT: 165}
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

_fonts = {}


def font(size, bold=False):
    key = (size, bold)
    if key not in _fonts:
        _fonts[key] = pygame.font.SysFont("microsoftyahei", size, bold=bold)
    return _fonts[key]


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def lerp(a, b, t):
    return a + (b - a) * t


def mix_color(c1, c2, t):
    t = clamp(t, 0.0, 1.0)
    return (int(lerp(c1[0], c2[0], t)),
            int(lerp(c1[1], c2[1], t)),
            int(lerp(c1[2], c2[2], t)))


def note_name(midi):
    midi = int(round(clamp(midi, 12, 108)))
    return NOTE_NAMES[midi % 12] + str(midi // 12 - 1)


def prop_color(kind, value):
    base = KIND_COLOR[kind]
    if kind == PITCH:
        t = clamp((value - 36.0) / 48.0, 0.0, 1.0)
        return mix_color((255, 126, 54), (255, 229, 140), t)
    if kind == ADSR:
        return mix_color((44, 150, 160), (110, 240, 214), clamp(value, 0.0, 1.0))
    return mix_color((140, 66, 190), (238, 150, 255), clamp(value, 0.0, 1.0))


def target_ratios():
    vals = (SETTINGS.ratio_pitch, SETTINGS.ratio_adsr, SETTINGS.ratio_effect)
    total = sum(vals)
    if total <= 1e-9:
        return (1.0 / 3.0,) * 3
    return tuple(v / total for v in vals)


def random_prop(kind):
    if kind == PITCH:
        return Prop(kind, random.uniform(36, 84))
    if kind == ADSR:
        return Prop(kind, random.uniform(0.25, 0.95))
    return Prop(kind, random.uniform(0.35, 0.95))


class Prop:
    __slots__ = ("kind", "value")

    def __init__(self, kind, value):
        self.kind = kind
        self.value = value

    def copy(self):
        return Prop(self.kind, self.value)

    def color(self):
        return prop_color(self.kind, self.value)

    def describe(self):
        if self.kind == PITCH:
            return note_name(self.value)
        if self.kind == ADSR:
            return f"ADSR {self.value:.2f}"
        return f"REV {int(self.value * 100)}%"


class Segment:
    __slots__ = ("a", "b", "n", "mid", "length", "prop", "strength")

    def __init__(self, a, b, n):
        self.a = a
        self.b = b
        self.n = n
        self.mid = (a + b) * 0.5
        self.length = a.distance_to(b)
        self.prop = None
        self.strength = 0.0


class Field:
    __slots__ = ("pos", "prop", "radius", "strength", "age", "life")

    def __init__(self, pos, prop, radius, life):
        self.pos = Vector2(pos)
        self.prop = prop.copy()
        self.radius = radius
        self.strength = 1.0
        self.age = 0.0
        self.life = life


class Wave:
    __slots__ = ("pos", "radius", "speed", "prop", "amp", "gen",
                 "reflected", "hit_balls", "age")

    def __init__(self, pos, prop, amp, gen=0):
        self.pos = Vector2(pos)
        self.radius = 6.0
        self.speed = 250.0 if prop.kind == EFFECT else 330.0
        self.prop = prop.copy()
        self.amp = amp
        self.gen = gen
        self.reflected = False
        self.hit_balls = set()
        self.age = 0.0


class Booster:
    __slots__ = ("pos", "r", "cd", "phase")

    def __init__(self, pos):
        self.pos = Vector2(pos)
        self.r = 13.0
        self.cd = 0.0
        self.phase = random.uniform(0, math.tau)


class Ball:
    __slots__ = ("pos", "vel", "r", "r0", "prop", "wall_cd", "ball_cd",
                 "flash", "wave_boost", "phase", "uid")

    def __init__(self, pos, vel, prop):
        self.pos = Vector2(pos)
        self.vel = Vector2(vel)
        self.r0 = random.uniform(6.0, 8.5)
        self.r = self.r0 * SETTINGS.ball_scale
        self.prop = prop
        self.wall_cd = 0.0
        self.ball_cd = 0.0
        self.flash = 0.0
        self.wave_boost = {}
        self.phase = random.uniform(0, math.tau)
        self.uid = random.getrandbits(32)

    def influence(self, fields):
        out = {}
        for f in fields:
            d = self.pos.distance_to(f.pos)
            if d < f.radius:
                w = f.strength * (1.0 - d / f.radius) ** 1.6
                if f.prop.kind not in out or w > out[f.prop.kind]:
                    out[f.prop.kind] = w
        for kind, (amp, _timer) in self.wave_boost.items():
            if amp > out.get(kind, 0.0):
                out[kind] = amp
        return out

    def effective(self, fields):
        infl = self.influence(fields)
        if self.prop.kind == PITCH:
            shift = infl.get(PITCH, 0.0) * 18.0
            return clamp(self.prop.value - shift, 12, 108), infl
        if self.prop.kind == ADSR:
            scale = 1.0 - infl.get(ADSR, 0.0) * 0.55
            return clamp(self.prop.value * scale, 0.05, 1.0), infl
        wet = clamp(self.prop.value + infl.get(EFFECT, 0.0) * 0.9, 0.0, 1.0)
        return wet, infl


def build_boundary(shape, scale=1.0):
    cx = CANVAS.centerx
    cy = CANVAS.centery
    pad = 10
    rx = CANVAS.width * 0.5 - pad
    ry = CANVAS.height * 0.5 - pad
    base = []

    if shape == "circle":
        r = min(rx, ry)
        n = 200
        base = [Vector2(cx + r * math.cos(math.tau * i / n),
                        cy + r * math.sin(math.tau * i / n)) for i in range(n)]
    elif shape == "ellipse":
        n = 200
        base = [Vector2(cx + rx * math.cos(math.tau * i / n),
                        cy + ry * math.sin(math.tau * i / n)) for i in range(n)]
    elif shape == "rect":
        x0, y0 = CANVAS.x + 8, CANVAS.y + 8
        x1, y1 = CANVAS.right - 8, CANVAS.bottom - 8
        corners = [Vector2(x0, y0), Vector2(x1, y0), Vector2(x1, y1), Vector2(x0, y1)]
        for i in range(4):
            a, b = corners[i], corners[(i + 1) % 4]
            steps = max(2, int(a.distance_to(b) / 24))
            for k in range(steps):
                base.append(a.lerp(b, k / steps))
    else:
        r = min(rx, ry)
        sides = 6
        verts = [Vector2(cx + r * math.cos(-math.pi / 2 + math.tau * i / sides),
                         cy + r * math.sin(-math.pi / 2 + math.tau * i / sides))
                 for i in range(sides)]
        for i in range(sides):
            a, b = verts[i], verts[(i + 1) % sides]
            steps = max(2, int(a.distance_to(b) / 22))
            for k in range(steps):
                base.append(a.lerp(b, k / steps))

    if scale != 1.0:
        center = Vector2(cx, cy)
        base = [center + (p - center) * scale for p in base]

    centroid = sum(base, Vector2(0, 0)) / len(base)
    segments = []
    for i in range(len(base)):
        a = base[i]
        b = base[(i + 1) % len(base)]
        edge = b - a
        if edge.length_squared() < 1e-6:
            continue
        n = Vector2(-edge.y, edge.x).normalize()
        if n.dot(centroid - a) < 0:
            n = -n
        segments.append(Segment(a, b, n))

    mask = pygame.Surface((W, H), pygame.SRCALPHA)
    pygame.draw.polygon(mask, (255, 255, 255, 255), base)
    return segments, mask, base


class World:
    def __init__(self, shape="circle"):
        self.shape = shape
        self.segments, self.mask, self.points = build_boundary(shape, SETTINGS.field_scale)
        self.balls = []
        self.boosters = []
        self.fields = []
        self.waves = []
        self.sparks = []
        self.speed_links = []
        self.type_guard_timer = 0.0
        self.paused = False
        self.elapsed = 0.0
        self.glow_cache = {}
        self.reset()

    def reset(self):
        self.segments, self.mask, self.points = build_boundary(self.shape, SETTINGS.field_scale)
        self.fields.clear()
        self.waves.clear()
        self.sparks.clear()
        self.balls.clear()
        self.boosters.clear()
        for _ in range(12):
            self.spawn_ball()
        self.ensure_types()
        for _ in range(2):
            self.spawn_booster()

    def ensure_types(self):
        have = {b.prop.kind for b in self.balls}
        for kind in (EFFECT, ADSR):
            if kind not in have:
                self.spawn_ball(kind)

    def set_shape(self, shape):
        if shape == self.shape:
            return
        self.shape = shape
        self.segments, self.mask, self.points = build_boundary(shape, SETTINGS.field_scale)
        self.fields.clear()
        self.waves.clear()
        self.sparks.clear()
        for b in self.balls:
            if not self.inside(b.pos, b.r + 1):
                b.pos = self.random_point(b.r + 4)
        for bo in self.boosters:
            if not self.inside(bo.pos, bo.r + 4):
                bo.pos = self.random_point(bo.r + 10)
        self.ensure_types()

    def set_field_scale(self, scale):
        old = SETTINGS.field_scale
        if abs(scale - old) < 1e-4:
            return
        SETTINGS.field_scale = scale
        ratio = scale / old if old > 0 else 1.0
        center = Vector2(CANVAS.center)
        self.segments, self.mask, self.points = build_boundary(self.shape, scale)
        self.fields.clear()
        self.waves.clear()
        self.sparks.clear()
        for b in self.balls:
            b.pos = center + (b.pos - center) * ratio
            if not self.inside(b.pos, b.r + 1):
                b.pos = self.random_point(b.r + 4)
        for bo in self.boosters:
            bo.pos = center + (bo.pos - center) * ratio
            if not self.inside(bo.pos, bo.r + 4):
                bo.pos = self.random_point(bo.r + 10)

    def set_ball_scale(self, scale):
        SETTINGS.ball_scale = scale
        for b in self.balls:
            b.r = max(2.0, b.r0 * scale)

    def spawn_booster(self, pos=None):
        if len(self.boosters) >= 5:
            return
        if pos is None:
            pos = self.random_point(26)
        self.boosters.append(Booster(pos))

    def inside(self, p, margin=0.0):
        for s in self.segments:
            if s.n.dot(p - s.a) < margin:
                return False
        return True

    def random_point(self, margin=0.0):
        for _ in range(200):
            p = Vector2(random.uniform(CANVAS.x, CANVAS.right),
                        random.uniform(CANVAS.y, CANVAS.bottom))
            if self.inside(p, margin):
                return p
        return Vector2(CANVAS.center)

    def spawn_ball(self, kind=None, pos=None):
        if len(self.balls) >= 32:
            return
        if kind is None:
            kind = random.choices([PITCH, ADSR, EFFECT], weights=target_ratios())[0]
        if pos is None:
            pos = self.random_point(12)
        angle = random.uniform(0, math.tau)
        speed = random.uniform(75, 185)
        if kind == PITCH:
            value = random.uniform(36, 84)
        elif kind == ADSR:
            value = random.uniform(0.25, 0.95)
        else:
            value = random.uniform(0.35, 0.95)
        self.balls.append(Ball(pos, Vector2(math.cos(angle), math.sin(angle)) * speed,
                               Prop(kind, value)))

    def add_field(self, pos, prop, radius, life=6.0):
        radius = max(24.0, radius * SETTINGS.wave_range)
        self.fields.append(Field(pos, prop, radius, life))
        if len(self.fields) > 50:
            del self.fields[0]

    def add_wave(self, pos, prop, amp, gen=0):
        if len(self.waves) > 48 or amp < 0.06:
            return
        self.waves.append(Wave(pos, prop, amp, gen))

    def add_sparks(self, pos, color, count=8, speed=160):
        for _ in range(count):
            a = random.uniform(0, math.tau)
            v = random.uniform(speed * 0.25, speed)
            self.sparks.append({
                "pos": Vector2(pos),
                "vel": Vector2(math.cos(a), math.sin(a)) * v,
                "life": random.uniform(0.18, 0.45),
                "max": 0.45,
                "color": color,
            })
        if len(self.sparks) > 260:
            del self.sparks[:len(self.sparks) - 260]

    def bounce_off_wall(self, ball):
        best = None
        best_pen = 0.0
        best_closest = None
        for s in self.segments:
            closest = self.closest_on_segment(ball.pos, s)
            d = ball.pos.distance_to(closest)
            if d < ball.r:
                pen = ball.r - d
                if pen > best_pen:
                    best_pen = pen
                    best = s
                    best_closest = closest
        if best is None:
            return False

        if best_closest is not None and ball.pos.distance_to(best_closest) > 1e-4:
            n = (ball.pos - best_closest).normalize()
        else:
            n = best.n.copy()

        ball.pos += n * (best_pen + 0.6)
        vn = ball.vel.dot(n)
        if vn < 0:
            ball.vel -= n * (2.0 * vn)

        if ball.wall_cd > 0:
            return True
        ball.wall_cd = 0.14

        old = ball.prop.copy()
        if best.prop is not None and best.strength > 0.05:
            ball.prop = best.prop.copy()
            best.prop = old
            best.strength = 1.0
            self.add_sparks(best_closest, old.color(), 10, 190)
        else:
            best.prop = old
            best.strength = 1.0
            self.add_sparks(best_closest, old.color(), 6, 140)

        self.rebalance_type(ball)
        ball.flash = 0.22
        self.add_field(best_closest, old, KIND_RADIUS[old.kind], 7.5)
        if old.kind == EFFECT:
            self.add_wave(best_closest, old, 1.0)
        else:
            self.add_wave(best_closest, old, 0.55)
        return True

    @staticmethod
    def closest_on_segment(p, s):
        ab = s.b - s.a
        denom = ab.length_squared()
        if denom < 1e-9:
            return s.a
        t = clamp((p - s.a).dot(ab) / denom, 0.0, 1.0)
        return s.a + ab * t

    def ensure_inside(self, ball):
        worst = None
        worst_d = float("inf")
        for s in self.segments:
            d = s.n.dot(ball.pos - s.a)
            if d < worst_d:
                worst_d = d
                worst = s

        if worst is None or worst_d >= 0.0:
            return

        closest = self.closest_on_segment(ball.pos, worst)
        ball.pos = closest + worst.n * (ball.r + 0.8)
        vn = ball.vel.dot(worst.n)
        if vn < 0.0:
            ball.vel -= worst.n * (2.0 * vn)

    def share_speeds(self, dt):
        self.speed_links.clear()
        balls = self.balls
        n = len(balls)
        if n < 2:
            return

        radius = 170.0 * SETTINGS.speed_share
        for i in range(n):
            a = balls[i]
            for j in range(i + 1, n):
                b = balls[j]
                d = a.pos.distance_to(b.pos)
                if d >= radius:
                    continue

                sa = a.vel.length()
                sb = b.vel.length()
                diff = sa - sb
                if abs(diff) < 6.0:
                    continue

                falloff = (1.0 - d / radius) ** 1.2
                transfer = (abs(diff) * 0.5
                            * min(1.0, 2.0 * dt * SETTINGS.speed_strength)
                            * falloff)
                dir_a = a.vel / sa if sa > 1e-6 else Vector2(0, 0)
                dir_b = b.vel / sb if sb > 1e-6 else Vector2(0, 0)

                if diff > 0.0:
                    a.vel = dir_a * max(0.0, sa - transfer)
                    b.vel = dir_b * (sb + transfer) if sb > 1e-6 else dir_a * transfer
                else:
                    b.vel = dir_b * max(0.0, sb - transfer)
                    a.vel = dir_a * (sa + transfer) if sa > 1e-6 else dir_b * transfer

                if len(self.speed_links) < 48:
                    self.speed_links.append((a.pos, b.pos, min(1.0, transfer / 80.0)))

        total = sum(b.vel.length() for b in balls)
        max_total = SETTINGS.speed_cap * n
        if total > max_total and total > 1e-6:
            k = max_total / total
            for b in balls:
                b.vel *= k

    def rebalance_type(self, ball):
        n = len(self.balls)
        if n < 6:
            return

        counts = {kind: 0 for kind in (PITCH, ADSR, EFFECT)}
        for b in self.balls:
            counts[b.prop.kind] += 1

        ratios = target_ratios()
        kinds = (PITCH, ADSR, EFFECT)
        over = []
        under = []
        for i, kind in enumerate(kinds):
            desired = ratios[i] * n
            if counts[kind] > desired + 0.4:
                over.append(kind)
            elif counts[kind] < desired - 0.4:
                under.append(kind)

        if ball.prop.kind not in over or not under:
            return

        index = kinds.index(ball.prop.kind)
        surplus = counts[ball.prop.kind] - ratios[index] * n
        if random.random() > min(0.75, 0.18 * surplus):
            return

        deficit = {kind: ratios[kinds.index(kind)] * n - counts[kind] for kind in under}
        new_kind = max(deficit, key=deficit.get)
        ball.prop = random_prop(new_kind)
        self.add_sparks(ball.pos, KIND_COLOR[new_kind], 4, 90)

    def enforce_type_presence(self):
        n = len(self.balls)
        if n < 6:
            return

        kinds = (PITCH, ADSR, EFFECT)
        counts = {kind: 0 for kind in kinds}
        for b in self.balls:
            counts[b.prop.kind] += 1

        missing = [kind for kind in kinds if counts[kind] == 0]
        if not missing:
            return

        most = max(kinds, key=lambda k: counts[k])
        if counts[most] <= 1:
            return

        ratios = target_ratios()
        target = max(missing, key=lambda k: ratios[kinds.index(k)])
        amount = max(1, min(2, int(round(ratios[kinds.index(target)] * n * 0.4))))

        candidates = [b for b in self.balls if b.prop.kind == most]
        random.shuffle(candidates)
        for b in candidates[:amount]:
            b.prop = random_prop(target)
            b.flash = 0.3
            self.add_sparks(b.pos, KIND_COLOR[target], 8, 150)
            self.add_wave(b.pos, b.prop, 0.45)

    def update(self, dt):
        if self.paused:
            return

        self.elapsed += dt

        self.type_guard_timer -= dt
        if self.type_guard_timer <= 0.0:
            self.type_guard_timer = 0.75
            self.enforce_type_presence()

        for bo in self.boosters:
            bo.cd = max(0.0, bo.cd - dt)

        for b in self.balls:
            b.wall_cd = max(0.0, b.wall_cd - dt)
            b.ball_cd = max(0.0, b.ball_cd - dt)
            b.flash = max(0.0, b.flash - dt)
            for kind in list(b.wave_boost):
                amp, timer = b.wave_boost[kind]
                timer -= dt
                amp *= math.exp(-dt / 0.65)
                if timer <= 0 or amp < 0.02:
                    del b.wave_boost[kind]
                else:
                    b.wave_boost[kind] = (amp, timer)

            disp = b.vel * dt * SETTINGS.ball_speed
            steps = max(1, int(math.ceil(disp.length() / max(3.0, b.r * 0.8))))
            for _ in range(steps):
                b.pos += disp / steps
                self.bounce_off_wall(b)

            for bo in self.boosters:
                if bo.cd > 0:
                    continue
                delta = b.pos - bo.pos
                dist = delta.length()
                if dist < b.r + bo.r:
                    n = delta / dist if dist > 1e-6 else Vector2(1, 0)
                    b.pos = bo.pos + n * (b.r + bo.r + 0.8)
                    b.vel += n * 95.0
                    speed = b.vel.length()
                    if speed > 400.0:
                        b.vel *= 400.0 / speed
                    elif speed < 90.0 and speed > 1e-6:
                        b.vel *= 90.0 / speed
                    bo.cd = 0.8
                    b.flash = 0.2
                    self.add_sparks(b.pos, (255, 226, 120), 6, 130)

        # 球与球
        n = len(self.balls)
        for i in range(n):
            a = self.balls[i]
            for j in range(i + 1, n):
                b = self.balls[j]
                delta = b.pos - a.pos
                dist = delta.length()
                min_d = a.r + b.r
                if dist < 1e-6 or dist >= min_d:
                    continue

                nrm = delta / dist
                overlap = min_d - dist
                a.pos -= nrm * (overlap * 0.5)
                b.pos += nrm * (overlap * 0.5)

                rel = b.vel - a.vel
                vn = rel.dot(nrm)
                if vn < 0:
                    ma = max(1.0, a.r * a.r)
                    mb = max(1.0, b.r * b.r)
                    j = -2.0 * vn / (1.0 / ma + 1.0 / mb)
                    a.vel -= nrm * (j / ma)
                    b.vel += nrm * (j / mb)

                if a.ball_cd <= 0 and b.ball_cd <= 0 and abs(vn) > 25:
                    a.ball_cd = 0.12
                    b.ball_cd = 0.12
                    mid = (a.pos + b.pos) * 0.5
                    a.flash = 0.18
                    b.flash = 0.18

                    src = a if a.prop.kind == EFFECT or b.prop.kind != EFFECT else b
                    self.add_field(mid, src.prop, KIND_RADIUS[src.prop.kind] * 0.55, 4.0)
                    if a.prop.kind != b.prop.kind:
                        other = b if src is a else a
                        self.add_field(mid, other.prop,
                                       KIND_RADIUS[other.prop.kind] * 0.5, 3.5)

                    for ball in (a, b):
                        if ball.prop.kind == EFFECT:
                            self.add_wave(mid, ball.prop, 0.7)
                        else:
                            self.add_wave(mid, ball.prop, 0.35)
                    self.add_sparks(mid, src.prop.color(), 5, 120)

        for ball in self.balls:
            self.ensure_inside(ball)

        self.share_speeds(dt)

        # 场的衰减
        for f in self.fields:
            f.age += dt
            f.strength = max(0.0, 1.0 - f.age / f.life)
        self.fields = [f for f in self.fields if f.strength > 0.01]

        # 墙结点缓慢衰减
        for s in self.segments:
            if s.prop is not None:
                s.strength *= math.exp(-dt / 38.0)
                if s.strength < 0.02:
                    s.prop = None
                    s.strength = 0.0

        # 水波
        for w in self.waves:
            w.age += dt
            step = w.speed * SETTINGS.wave_speed * dt
            w.radius += step
            w.amp *= math.exp(-dt / 2.6)

            for b in self.balls:
                if b.uid in w.hit_balls:
                    continue
                d = b.pos.distance_to(w.pos)
                if abs(d - w.radius) < 11.0 + b.r:
                    w.hit_balls.add(b.uid)
                    b.wave_boost[w.prop.kind] = (max(w.amp * 0.75, 0.15), 1.6)

            if not w.reflected and w.age > 0.10 and w.gen < 2:
                hits = []
                for s in self.segments:
                    d = s.mid.distance_to(w.pos)
                    if abs(d - w.radius) < step + 3.0:
                        hits.append(s)
                if hits:
                    w.reflected = True
                    chosen = []
                    for s in hits:
                        ang = math.atan2(s.mid.y - w.pos.y, s.mid.x - w.pos.x)
                        if all(abs((ang - c + math.pi) % math.tau - math.pi) > 1.35
                               for c in chosen):
                            chosen.append(ang)
                            self.add_wave(s.mid, w.prop, w.amp * 0.52, w.gen + 1)
                            self.add_sparks(s.mid, w.prop.color(), 3, 70)
                        if len(chosen) >= 2:
                            break

        max_radius = min(720.0, 900.0 * SETTINGS.wave_range)
        self.waves = [w for w in self.waves if w.amp > 0.04 and w.radius < max_radius]

        # 火花
        for sp in self.sparks:
            sp["life"] -= dt
            sp["pos"] += sp["vel"] * dt
            sp["vel"] *= math.exp(-dt * 2.2)
        self.sparks = [s for s in self.sparks if s["life"] > 0]

    def glow(self, kind):
        if kind not in self.glow_cache:
            surf = pygame.Surface((128, 128), pygame.SRCALPHA)
            c = KIND_COLOR[kind]
            for r in range(63, 0, -1):
                a = int(22 * (1.0 - r / 63.0) ** 2.2)
                pygame.draw.circle(surf, (c[0], c[1], c[2], a), (64, 64), r)
            self.glow_cache[kind] = surf
        return self.glow_cache[kind]

    def draw(self, screen, fx):
        fx.fill((0, 0, 0, 0))

        for f in self.fields:
            g = self.glow(f.prop.kind)
            size = max(8, int(f.radius * (0.55 + 0.45 * f.strength) * 2))
            scaled = pygame.transform.scale(g, (size, size))
            scaled.set_alpha(int(170 * f.strength))
            fx.blit(scaled, scaled.get_rect(center=f.pos))

        for w in self.waves:
            c = w.prop.color()
            a = int(120 * clamp(w.amp, 0, 1))
            if a <= 2:
                continue
            pos = (int(w.pos.x), int(w.pos.y))
            width = 3 if w.prop.kind == EFFECT else 2
            pygame.draw.circle(fx, (*c, a), pos, int(w.radius), width)
            if w.prop.kind == EFFECT:
                pygame.draw.circle(fx, (*c, a // 2), pos, max(1, int(w.radius - 14)), 2)
                pygame.draw.circle(fx, (*c, a // 4), pos, max(1, int(w.radius - 28)), 2)

        for p1, p2, amount in self.speed_links:
            col = (145, 212, 255, int(95 + 140 * amount))
            pygame.draw.line(fx, col,
                             (int(p1.x), int(p1.y)), (int(p2.x), int(p2.y)), 2)

        for sp in self.sparks:
            a = int(255 * clamp(sp["life"] / sp["max"], 0, 1))
            c = sp["color"]
            pygame.draw.circle(fx, (*c, a), (int(sp["pos"].x), int(sp["pos"].y)), 2)

        fx.blit(self.mask, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
        screen.blit(fx, (0, 0))

        for s in self.segments:
            if s.prop is None or s.strength < 0.02:
                col = WALL
                w = 2
            else:
                col = mix_color(WALL, s.prop.color(), 0.25 + 0.75 * s.strength)
                w = 3 if s.strength > 0.25 else 2
            pygame.draw.line(screen, col,
                             (int(s.a.x), int(s.a.y)),
                             (int(s.b.x), int(s.b.y)), w)

        for bo in self.boosters:
            self.draw_booster(screen, bo)

        for b in self.balls:
            self.draw_ball(screen, b)

    def draw_booster(self, screen, bo):
        pulse = 0.5 + 0.5 * math.sin(self.elapsed * 4.0 + bo.phase)
        col = (255, 226, 120) if bo.cd <= 0 else (150, 142, 104)
        cx, cy = int(bo.pos.x), int(bo.pos.y)
        pygame.draw.circle(screen, (18, 21, 27), (cx, cy), int(bo.r + 4))
        pygame.draw.circle(screen, col, (cx, cy), int(bo.r + 1 + pulse * 2), 2)
        for k in range(4):
            ang = self.elapsed * 0.8 + k * math.pi / 2
            p1 = bo.pos + Vector2(math.cos(ang), math.sin(ang)) * (bo.r + 4)
            p2 = bo.pos + Vector2(math.cos(ang), math.sin(ang)) * (bo.r + 10 + pulse * 3)
            pygame.draw.line(screen, col,
                             (int(p1.x), int(p1.y)), (int(p2.x), int(p2.y)), 2)
        pygame.draw.circle(screen, col, (cx, cy), 3)

    def draw_ball(self, screen, b):
        value, infl = b.effective(self.fields)
        base_col = prop_color(b.prop.kind, value if b.prop.kind != PITCH else b.prop.value)
        if b.prop.kind == PITCH:
            base_col = prop_color(PITCH, value)

        g = self.glow(b.prop.kind)
        gr = int(b.r * 5.2)
        scaled = pygame.transform.smoothscale(g, (gr, gr))
        scaled.set_alpha(110)
        screen.blit(scaled, scaled.get_rect(center=b.pos))

        strongest = max(infl.items(), key=lambda kv: kv[1])[0] if infl else None
        if strongest is not None and b.prop.kind != strongest:
            aura = KIND_COLOR[strongest]
            pygame.draw.circle(screen, (*aura, 150),
                               (int(b.pos.x), int(b.pos.y)),
                               int(b.r + 4 + 2 * math.sin(self.elapsed * 7 + b.phase)), 1)
        elif infl:
            aura = KIND_COLOR[b.prop.kind]
            pygame.draw.circle(screen, (*aura, 110),
                               (int(b.pos.x), int(b.pos.y)),
                               int(b.r + 4 + 2 * math.sin(self.elapsed * 7 + b.phase)), 1)

        if b.prop.kind == EFFECT:
            pulse = b.r + 5 + 3 * (0.5 + 0.5 * math.sin(self.elapsed * 5 + b.phase))
            pygame.draw.circle(screen, (*KIND_COLOR[EFFECT], 90),
                               (int(b.pos.x), int(b.pos.y)), int(pulse), 1)

        core = mix_color(base_col, (255, 255, 255), 0.25 if b.flash > 0 else 0.0)
        pygame.draw.circle(screen, (8, 10, 14), (int(b.pos.x), int(b.pos.y)), int(b.r + 2))
        pygame.draw.circle(screen, core, (int(b.pos.x), int(b.pos.y)), int(b.r))
        pygame.draw.circle(screen, (255, 255, 255), (int(b.pos.x), int(b.pos.y)),
                           int(b.r), 1)

        mouse = Vector2(pygame.mouse.get_pos())
        hover = b.pos.distance_to(mouse) < max(18.0, b.r + 14)
        if b.r < 5.2 and not hover:
            return

        if b.prop.kind == PITCH:
            text = note_name(value)
        elif b.prop.kind == ADSR:
            text = "ADSR"
        else:
            text = "REV"
        f = font(11, True)
        surf = f.render(text, True, (10, 12, 16))
        pad = 3
        rect = surf.get_rect(center=(b.pos.x, b.pos.y - b.r - 11))
        bg = pygame.Surface((rect.width + pad * 2, rect.height + 2), pygame.SRCALPHA)
        pygame.draw.rect(bg, (*base_col, 225), bg.get_rect(), border_radius=4)
        screen.blit(bg, (rect.x - pad, rect.y - 1))
        screen.blit(surf, rect)


class UISlider:
    def __init__(self, label, rect, min_value, max_value, value, callback):
        self.label = label
        self.rect = pygame.Rect(rect)
        self.min_value = min_value
        self.max_value = max_value
        self.value = value
        self.callback = callback
        self.dragging = False

    def set_from_x(self, x):
        t = clamp((x - self.rect.x) / max(1, self.rect.width), 0.0, 1.0)
        self.value = lerp(self.min_value, self.max_value, t)
        self.callback(self.value)

    def draw(self, screen):
        lab = font(14, True).render(self.label, True, TEXT)
        val = font(13, True).render(f"{int(round(self.value * 100))}%", True, ACCENT)
        screen.blit(lab, (self.rect.x, self.rect.y))
        screen.blit(val, (self.rect.right - val.get_width(), self.rect.y + 1))

        bar = pygame.Rect(self.rect.x, self.rect.bottom - 9, self.rect.width, 6)
        pygame.draw.rect(screen, (34, 40, 50), bar, border_radius=3)
        t = (self.value - self.min_value) / (self.max_value - self.min_value)
        fill = pygame.Rect(bar.x, bar.y, int(bar.width * t), bar.height)
        pygame.draw.rect(screen, ACCENT, fill, border_radius=3)
        hx = bar.x + int(bar.width * t)
        pygame.draw.circle(screen, (255, 222, 176), (hx, bar.centery), 6)
        pygame.draw.circle(screen, (12, 14, 18), (hx, bar.centery), 6, 2)


def draw_ratio_bar(screen, world, x, y, width):
    counts = {PITCH: 0, ADSR: 0, EFFECT: 0}
    for b in world.balls:
        counts[b.prop.kind] += 1
    total = max(1, len(world.balls))
    current = (counts[PITCH] / total, counts[ADSR] / total, counts[EFFECT] / total)
    target = target_ratios()

    title = font(13, True).render("球类型比例", True, TEXT)
    screen.blit(title, (x, y))

    bar_x = x + 38
    bar_w = width - 38
    for label, vals, yy in (("当前", current, y + 20), ("目标", target, y + 38)):
        lab = font(11).render(label, True, DIM)
        screen.blit(lab, (x, yy - 2))
        xx = bar_x
        for i, kind in enumerate((PITCH, ADSR, EFFECT)):
            seg = int(round(bar_w * vals[i]))
            if seg > 0:
                pygame.draw.rect(screen, KIND_COLOR[kind], (xx, yy, seg, 8))
            xx += seg
        pygame.draw.rect(screen, (46, 54, 66), (bar_x, yy, bar_w, 8), 1)

    cur_txt = f"{int(round(current[0] * 100))}% / {int(round(current[1] * 100))}% / {int(round(current[2] * 100))}%"
    tgt_txt = f"目标 {int(round(target[0] * 100))}% / {int(round(target[1] * 100))}% / {int(round(target[2] * 100))}%"
    screen.blit(font(11).render(cur_txt + "    " + tgt_txt, True, DIM), (bar_x, y + 51))


def draw_ui(screen, world, sliders, fps=0.0):
    pygame.draw.rect(screen, PANEL, (0, H - PANEL_H, W, PANEL_H))
    pygame.draw.line(screen, (33, 39, 49), (0, H - PANEL_H), (W, H - PANEL_H), 1)

    title = font(30, True).render("随机合成器 · 视觉原型", True, TEXT)
    screen.blit(title, (44, 26))
    sub = font(14).render("碰撞 → 性质交换 → 辐射场 → 水波反弹", True, DIM)
    screen.blit(sub, (46, 70))

    shape_names = {"circle": "圆形", "rect": "矩形", "ellipse": "椭圆", "hex": "六边形"}
    avg_speed = (sum(b.vel.length() for b in world.balls) / len(world.balls)
                 if world.balls else 0.0)
    info = font(15).render(
        f"{shape_names[world.shape]}   BALLS {len(world.balls):02d}   "
        f"BOOSTS {len(world.boosters)}   FIELDS {len(world.fields):02d}   "
        f"WAVES {len(world.waves):02d}   SPD {avg_speed:.0f}/{SETTINGS.speed_cap:.0f}   "
        f"FPS {fps:.0f}",
        True, DIM)
    screen.blit(info, (W - info.get_width() - 44, 44))

    # 图例
    x = 46
    y = H - PANEL_H + 12
    for kind in (PITCH, ADSR, EFFECT):
        c = KIND_COLOR[kind]
        pygame.draw.circle(screen, c, (x + 7, y + 8), 6)
        label = font(14, True).render(
            {"pitch": "音高球", "adsr": "ADSR 球", "effect": "效果球"}[kind],
            True, TEXT)
        screen.blit(label, (x + 20, y))
        x += label.get_width() + 52

    for slider in sliders:
        slider.draw(screen)

    draw_ratio_bar(screen, world, 990, H - PANEL_H + 108, W - 990 - 46)

    hint1 = "SPACE 暂停    R 重置    N 加 5 球    B 加加速器    左键 加球    右键 加效果球"
    hint2 = "1-4 形状    C 清空场    球很小时悬停看性质    类型比例会自动柔和配平"
    screen.blit(font(14).render(hint1, True, DIM), (46, H - PANEL_H + 178))
    screen.blit(font(14).render(hint2, True, DIM), (46, H - PANEL_H + 202))

    tip1 = font(13).render("球撞墙会与墙结点交换性质", True, (110, 119, 134))
    tip2 = font(13).render("快球会把速度分给范围内的慢球", True, (110, 119, 134))
    screen.blit(tip1, (W - tip1.get_width() - 44, H - PANEL_H + 178))
    screen.blit(tip2, (W - tip2.get_width() - 44, H - PANEL_H + 202))

    if world.paused:
        overlay = pygame.Surface((W, H), pygame.SRCALPHA)
        overlay.fill((5, 7, 10, 120))
        screen.blit(overlay, (0, 0))
        t = font(46, True).render("PAUSED", True, TEXT)
        screen.blit(t, t.get_rect(center=(W // 2, H // 2 - 40)))
        t2 = font(15).render("按 SPACE 继续", True, DIM)
        screen.blit(t2, t2.get_rect(center=(W // 2, H // 2 + 10)))


def main():
    pygame.init()
    pygame.display.set_caption("随机合成器 · 视觉原型")
    screen = pygame.display.set_mode((W, H))
    clock = pygame.time.Clock()
    fx = pygame.Surface((W, H), pygame.SRCALPHA)

    world = World("circle")

    row1_y = H - PANEL_H + 40
    w1 = (W - 92 - 6 * 12) // 7
    r1x = lambda i: 46 + i * (w1 + 12)
    sliders = [
        UISlider("场地大小", (r1x(0), row1_y, w1, 44),
                 0.5, 1.0, SETTINGS.field_scale, world.set_field_scale),
        UISlider("小球大小", (r1x(1), row1_y, w1, 44),
                 0.3, 1.6, SETTINGS.ball_scale, world.set_ball_scale),
        UISlider("小球速度", (r1x(2), row1_y, w1, 44),
                 0.1, 3.0, SETTINGS.ball_speed,
                 lambda v: setattr(SETTINGS, "ball_speed", v)),
        UISlider("调速范围", (r1x(3), row1_y, w1, 44),
                 0.4, 2.5, SETTINGS.speed_share,
                 lambda v: setattr(SETTINGS, "speed_share", v)),
        UISlider("调速强度", (r1x(4), row1_y, w1, 44),
                 0.1, 3.0, SETTINGS.speed_strength,
                 lambda v: setattr(SETTINGS, "speed_strength", v)),
        UISlider("水波速度", (r1x(5), row1_y, w1, 44),
                 0.2, 2.0, SETTINGS.wave_speed,
                 lambda v: setattr(SETTINGS, "wave_speed", v)),
        UISlider("水波范围", (r1x(6), row1_y, w1, 44),
                 0.2, 1.5, SETTINGS.wave_range,
                 lambda v: setattr(SETTINGS, "wave_range", v)),
    ]

    ratio_vals = [SETTINGS.ratio_pitch, SETTINGS.ratio_adsr, SETTINGS.ratio_effect]
    ratio_sliders = []

    def set_ratio(idx, value):
        value = clamp(value, 0.0, 1.0)
        ratio_vals[idx] = value
        others = [k for k in range(3) if k != idx]
        remaining = 1.0 - value
        other_sum = ratio_vals[others[0]] + ratio_vals[others[1]]
        if other_sum <= 1e-9:
            ratio_vals[others[0]] = remaining * 0.5
            ratio_vals[others[1]] = remaining * 0.5
        else:
            ratio_vals[others[0]] *= remaining / other_sum
            ratio_vals[others[1]] *= remaining / other_sum
        SETTINGS.ratio_pitch = ratio_vals[0]
        SETTINGS.ratio_adsr = ratio_vals[1]
        SETTINGS.ratio_effect = ratio_vals[2]
        for k, slider in enumerate(ratio_sliders):
            slider.value = ratio_vals[k]

    row2_y = H - PANEL_H + 108
    for i, label in enumerate(("音高比例", "ADSR 比例", "效果比例")):
        ratio_sliders.append(
            UISlider(label, (46 + i * 310, row2_y, 290, 44),
                     0.0, 1.0, ratio_vals[i],
                     lambda v, i=i: set_ratio(i, v)))

    sliders.extend(ratio_sliders)

    bg_grid = pygame.Surface((W, H))
    bg_grid.fill(BG)
    for gx in range(0, W, 32):
        for gy in range(0, H, 32):
            pygame.draw.circle(bg_grid, GRID, (gx, gy), 1)
    pygame.draw.rect(bg_grid, BG, (0, H - PANEL_H, W, PANEL_H))

    test_mode = "--test" in sys.argv
    frames = 0

    running = True
    while running:
        dt = min(clock.tick(60) / 1000.0, 0.033)
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key == pygame.K_SPACE:
                    world.paused = not world.paused
                elif event.key == pygame.K_r:
                    world.reset()
                elif event.key == pygame.K_n:
                    for _ in range(5):
                        world.spawn_ball()
                elif event.key == pygame.K_c:
                    world.fields.clear()
                    world.waves.clear()
                    world.sparks.clear()
                elif event.key == pygame.K_b:
                    world.spawn_booster()
                elif event.key == pygame.K_1:
                    world.set_shape("circle")
                elif event.key == pygame.K_2:
                    world.set_shape("rect")
                elif event.key == pygame.K_3:
                    world.set_shape("ellipse")
                elif event.key == pygame.K_4:
                    world.set_shape("hex")
            elif event.type == pygame.MOUSEMOTION:
                for slider in sliders:
                    if slider.dragging:
                        slider.set_from_x(event.pos[0])
            elif event.type == pygame.MOUSEBUTTONUP:
                for slider in sliders:
                    slider.dragging = False
            elif event.type == pygame.MOUSEBUTTONDOWN:
                used_slider = False
                for slider in sliders:
                    if slider.rect.collidepoint(event.pos):
                        slider.dragging = True
                        slider.set_from_x(event.pos[0])
                        used_slider = True
                        break

                if not used_slider and CANVAS.collidepoint(event.pos):
                    if event.button == 1 and world.inside(Vector2(event.pos), 12):
                        world.spawn_ball(pos=Vector2(event.pos))
                    elif event.button == 3 and world.inside(Vector2(event.pos), 12):
                        world.spawn_ball(EFFECT, Vector2(event.pos))

        world.update(dt)

        screen.blit(bg_grid, (0, 0))
        world.draw(screen, fx)
        draw_ui(screen, world, sliders, clock.get_fps())
        pygame.display.flip()

        if test_mode:
            frames += 1
            if frames > 150:
                running = False

    pygame.quit()


if __name__ == "__main__":
    main()
