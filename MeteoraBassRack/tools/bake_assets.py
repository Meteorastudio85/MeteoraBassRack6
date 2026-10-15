"""
Meteora Bass Rack - pipeline grafica v6 (immagine rack dell'utente + pedale Tube Smile)
coordinate logiche = pixel dell'immagine originale 1536x1024; alla fine si taglia la fascia
in alto (CROP_Y) dove c'era l'etichetta del generatore. Uscita a 2x.
"""
import cv2, numpy as np, json, os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

S = 2
CROP_Y = 85
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
RACK = sys.argv[1] if len(sys.argv) > 1 else '/mnt/user-data/uploads/copilot_image_1792041245600.jpeg'
PEDAL = sys.argv[2] if len(sys.argv) > 2 else '/mnt/user-data/uploads/copilot_image_1792014277517.jpeg'
LOGO = sys.argv[3] if len(sys.argv) > 3 else '/mnt/user-data/uploads/1792015509650.jpg'
OUT = sys.argv[4] if len(sys.argv) > 4 else '/home/claude/w/assets6'
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(6)

SRC = cv2.imread(RACK)
H0, W0 = SRC.shape[:2]
up = cv2.resize(SRC, (W0*S, H0*S), interpolation=cv2.INTER_LANCZOS4)
bl = cv2.GaussianBlur(up, (0,0), 1.8)
up = cv2.addWeighted(up, 1.35, bl, -0.35, 0)
ORIG = up.copy(); img = up.copy(); H, W = img.shape[:2]
P = lambda v: int(round(v*S))
def lum(a): return cv2.cvtColor(a, cv2.COLOR_BGR2GRAY).astype(np.float32)
FONTS = {
    'poppins': '/usr/share/fonts/truetype/google-fonts/Poppins-Medium.ttf',
    'poppinsb': '/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf',
    'sans':  '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',
    'sansb': '/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf',
    'cond':  '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf',
    'condb': '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',
}
exec(open(os.path.join(BASE_DIR, 'helpers6.py')).read())

def disc_mask(cx, cy, r, x0, y0, shape):
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    return np.hypot(xx - (P(cx)-x0), yy - (P(cy)-y0)) < P(r)

def fill_disc(cx, cy, r, seed, sigma=5):
    x0, y0 = P(cx-r-8), P(cy-r-8); x1, y1 = P(cx+r+8), P(cy+r+8)
    m = disc_mask(cx, cy, r, x0, y0, (y1-y0, x1-x0)).astype(np.float32)
    fill_region(x0, y0, x1, y1, cv2.GaussianBlur(m, (0,0), P(1.2)), sigma=sigma, seed=seed)

def fill_rect(a, b, c, d, seed, sigma=4, pad=8):
    x0, y0, x1, y1 = P(a-pad), P(b-pad), P(c+pad), P(d+pad)
    m = np.zeros((y1-y0, x1-x0), np.float32)
    m[P(pad):-P(pad), P(pad):-P(pad)] = 1
    fill_region(x0, y0, x1, y1, cv2.GaussianBlur(m, (0,0), P(1.2)), sigma=sigma, seed=seed)

def paste(patch_bgr, alpha, cx, cy):
    """incolla una patch (2x) centrata in (cx,cy) logici"""
    global img
    h, w = patch_bgr.shape[:2]
    x0, y0 = P(cx) - w//2, P(cy) - h//2
    dst = img[y0:y0+h, x0:x0+w].astype(np.float32)
    a = alpha[..., None]
    img[y0:y0+h, x0:x0+w] = np.clip(patch_bgr*a + dst*(1-a), 0, 255).astype(np.uint8)

def copy_knob(cx, cy, R, dcx, dcy, k, shadow=True):
    src = img[P(cy-R):P(cy+R), P(cx-R):P(cx+R)].astype(np.float32)
    D = int(round(src.shape[0]*k))
    src = cv2.resize(src, (D, D), interpolation=cv2.INTER_AREA)
    m = np.zeros((D, D), np.float32); cv2.circle(m, (D//2, D//2), int(D*0.47), 1.0, -1)
    m = cv2.GaussianBlur(m, (0,0), D*0.02)
    if shadow:
        sh = np.zeros((D, D), np.float32); cv2.circle(sh, (D//2+P(1), D//2+P(2)), int(D*0.40), 1.0, -1)
        sh = cv2.GaussianBlur(sh, (0,0), P(2.2))
        x0, y0 = P(dcx)-D//2, P(dcy)-D//2
        img[y0:y0+D, x0:x0+D] = (img[y0:y0+D, x0:x0+D].astype(np.float32)*(1-0.5*sh[...,None])).astype(np.uint8)
    paste(src, m, dcx, dcy)

# ======================================================================= 1. TUBE SMILE nuovo
# slot svuotato: pannello nero (texture presa dal VOG) con ombra interna
SX0, SY0, SX1, SY1 = 1257, 449, 1477, 912
donor = img[P(195):P(420), P(1125):P(1285)].astype(np.float32)
tile = np.concatenate([donor, donor[::-1]], axis=0)
tile = np.concatenate([tile, tile[::-1]], axis=0)
sw, sh_ = P(SX1)-P(SX0), P(SY1)-P(SY0)
tile = cv2.resize(tile, (sw, int(tile.shape[0]*sw/tile.shape[1])))[:sh_]
if tile.shape[0] < sh_:
    tile = np.concatenate([tile, tile[::-1]], axis=0)[:sh_]
yy, xx = np.mgrid[0:sh_, 0:sw].astype(np.float32)
inner = np.minimum.reduce([xx, yy, sw-1-xx, sh_-1-yy]) / P(18)
tile = tile * (0.55 + 0.45*np.clip(inner, 0, 1))[..., None]
img[P(SY0):P(SY1), P(SX0):P(SX1)] = np.clip(tile, 0, 255).astype(np.uint8)

PED = cv2.imread(PEDAL)
pa = PED.astype(np.int32)
nonwhite = (np.abs(pa - 255).sum(axis=2) > 45).astype(np.uint8)
body = np.zeros_like(nonwhite); body[118:1394, 133:893] = 1
pm = (nonwhite & body).astype(np.uint8)*255
pm = cv2.morphologyEx(pm, cv2.MORPH_CLOSE, np.ones((9,9), np.uint8))
cnts, _ = cv2.findContours(pm, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
cnt = max(cnts, key=cv2.contourArea)
pm = np.zeros_like(pm); cv2.drawContours(pm, [cnt], -1, 255, -1)
pm = cv2.erode(pm, np.ones((3,3), np.uint8), iterations=2)
PK = {'tsInput': (280, 262), 'tsLevel': (510, 262), 'tsDrive': (740, 262), 'tsBlend': (276, 880)}
ped = PED.copy()
# tolgo gli indici bianchi dei pomelli (li disegna il plugin)
for (kx, ky) in PK.values():
    yy, xx = np.mgrid[0:ped.shape[0], 0:ped.shape[1]]
    disk = np.hypot(xx-kx, yy-ky) < 74
    L = lum(ped)
    col = np.median(ped[disk & (L < 50)], axis=0)
    bright = disk & (L > 110)
    bright = cv2.dilate(bright.astype(np.uint8), np.ones((5,5), np.uint8), iterations=2).astype(bool) & disk
    ped[bright] = col
# luce come la foto del rack: meno esposizione, un filo più caldo
pf = ped.astype(np.float32)
pf = (pf - 128)*0.95 + 128
pf = pf * np.array([0.70, 0.76, 0.80])
yyp, xxp = np.mgrid[0:pf.shape[0], 0:pf.shape[1]].astype(np.float32)
vig = 1 - 0.18*np.clip(np.hypot((xxp-512)/520, (yyp-755)/760), 0, 1)**2
pf = pf * vig[..., None]
PSC = 0.2715
cx_ped, cy_ped = 512.5, 755.5
TSX, TSY = (SX0+SX1)/2, SY0 + 10 + 1271*0.2715/2
def ped_map(x, y): return (TSX + (x - cx_ped)*PSC, TSY + (y - cy_ped)*PSC)
k = PSC*S
x0b, y0b, x1b, y1b = 120, 105, 905, 1405
crop = pf[y0b:y1b, x0b:x1b]; cm = pm[y0b:y1b, x0b:x1b].astype(np.float32)/255
D_w, D_h = int(round(crop.shape[1]*k)), int(round(crop.shape[0]*k))
crop = cv2.resize(crop, (D_w, D_h), interpolation=cv2.INTER_AREA)
cm = cv2.GaussianBlur(cv2.resize(cm, (D_w, D_h), interpolation=cv2.INTER_AREA), (0,0), 0.7)
pcx, pcy = ped_map((x0b+x1b)/2, (y0b+y1b)/2)
# ombra del pedale sul pannello
shm = cv2.GaussianBlur(np.roll(np.roll(cm, P(3), 0), P(2), 1), (0,0), P(5))
gx0, gy0 = P(pcx) - D_w//2, P(pcy) - D_h//2
img[gy0:gy0+D_h, gx0:gx0+D_w] = (img[gy0:gy0+D_h, gx0:gx0+D_w].astype(np.float32)*(1-0.6*shm[...,None])).astype(np.uint8)
paste(crop, cm, pcx, pcy)
TS_KNOBS = {k_: ped_map(*v) for k_, v in PK.items()}
FS = ped_map(354, 1036) + ped_map(670, 1266)
# LED del pedale (spento nella base)
LEDX, LEDY = ped_map(748, 880)
def draw_led(bgr, cx, cy, r, lens, lit):
    """LED con ghiera cromata, disegnato a 2x su una copia"""
    out = bgr.astype(np.float32)
    h, w = out.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.hypot(xx - cx, yy - cy)
    ring = np.clip(1 - np.abs(d - r*1.25)/(r*0.35), 0, 1)
    shade = 0.55 + 0.45*np.clip((cy - yy)/(r*1.6) + 0.5, 0, 1)
    out = out*(1-ring[...,None]) + (np.array([175,178,182])*shade[...,None])*ring[...,None]
    a = np.clip((r - d)*1.2, 0, 1)
    base = np.array(lens, np.float32)
    core = (0.55 + 0.45*np.clip(1 - d/r, 0, 1))[..., None]
    out = out*(1-a[...,None]) + base*core*a[...,None]
    spec = np.exp(-(((xx-cx+r*0.35)/(r*0.3))**2 + ((yy-cy+r*0.4)/(r*0.22))**2))
    out = out + spec[...,None]*(120 if lit else 50)*a[...,None]
    if lit:
        glow = np.exp(-(d/(r*2.4))**2)[..., None]
        out = out + glow*np.array(lens, np.float32)*0.55
    return np.clip(out, 0, 255)
R_ = P(16)
x0l, y0l = P(LEDX)-R_, P(LEDY)-R_
reg = img[y0l:y0l+2*R_, x0l:x0l+2*R_]
img[y0l:y0l+2*R_, x0l:x0l+2*R_] = draw_led(reg, R_, R_, P(4.6), (30,25,90), False).astype(np.uint8)

# modulo OUTPUT (ultimo stadio della catena): piastra scura con viti, pomello, LED di picco
OX0, OY0, OX1, OY1 = 1263, 818, 1471, 906
pl = img[P(OY0):P(OY1), P(OX0):P(OX1)].astype(np.float32)
hh_, ww_ = pl.shape[:2]
gtex = fbm(hh_, ww_, ((0.6*S,0.5),(3*S,0.3),(12*S,0.2)), 901)
brushed = cv2.GaussianBlur(np.random.default_rng(902).standard_normal((hh_, ww_)).astype(np.float32), (0,0), sigmaX=P(6), sigmaY=0.6)
plate = np.ones((hh_, ww_, 3), np.float32) * np.array([30,31,33], np.float32)
plate = plate + gtex[...,None]*3.0 + brushed[...,None]*6.0
yy, xx = np.mgrid[0:hh_, 0:ww_].astype(np.float32)
edge = np.minimum.reduce([xx, yy, ww_-1-xx, hh_-1-yy])
plate = plate + (np.clip(1 - edge/P(1.5), 0, 1)*28)[..., None] * (yy < hh_/2)[..., None]      # filo di luce in alto
plate = plate * (1 - 0.35*np.clip(1 - edge/P(5), 0, 1))[..., None]
mk = np.zeros((hh_, ww_), np.float32); cv2.rectangle(mk, (P(1),P(1)), (ww_-P(1), hh_-P(1)), 1.0, -1)
mk = cv2.GaussianBlur(mk, (0,0), P(0.8))[..., None]
img[P(OY0):P(OY1), P(OX0):P(OX1)] = np.clip(plate*mk + pl*(1-mk), 0, 255).astype(np.uint8)
# viti agli angoli (copiate da una vite del rack)
scr = img[P(885-11):P(885+11), P(408-11):P(408+11)].astype(np.float32)
sm = soft_disc(scr.shape[0], scr.shape[1], scr.shape[0]*0.42, P(0.8))
for (sx, sy) in [(OX0+9, OY0+9), (OX1-9, OY0+9), (OX0+9, OY1-9), (OX1-9, OY1-9)]:
    sc_ = cv2.resize(scr, (P(12), P(12)), interpolation=cv2.INTER_AREA)
    paste(sc_, cv2.resize(sm, (P(12), P(12))), sx, sy)
# ======================================================================= 2. EQ: 550b -> 560b
fill_rect(248, 183, 360, 224, 11, sigma=3, pad=4)
blue = (214, 128, 40)   # BGR
paint_text([(304, 204, '570b', 'sans', 38, 0)], blue, 12, mode='silk', wear=0.25)
fill_disc(328, 795, 30, 13, sigma=3)   # pomellino senza funzione
# icona originale: piccola cometa (marchio Meteora) al posto del disegno a strisce
fill_rect(92, 180, 160, 244, 15, sigma=3, pad=4)
def comet_icon(cx, cy, size, color_rgb):
    global img
    K = 4; Wd = int(size*2.4*S*K); Hd = int(size*2.4*S*K)
    im = Image.new('L', (Wd, Hd), 0); d = ImageDraw.Draw(im)
    c = Wd*0.62; r = size*0.36*S*K
    d.ellipse((c-r, Hd*0.38-r, c+r, Hd*0.38+r), fill=255)
    # tre scie che si assottigliano verso il basso a sinistra
    for i, (off, ln, w) in enumerate([(-0.55, 1.55, 0.42), (0.0, 1.85, 0.55), (0.55, 1.35, 0.36)]):
        x0_, y0_ = c + off*r*0.9*0.7, Hd*0.38 + off*r*0.9*0.7
        dx, dy = -0.72, 0.69
        px_, py_ = -dy, dx
        L_ = ln*r*1.6; ww_ = w*r
        pts = [(x0_ + px_*ww_, y0_ + py_*ww_), (x0_ - px_*ww_, y0_ - py_*ww_),
               (x0_ + dx*L_ - px_*ww_*0.15, y0_ + dy*L_ - py_*ww_*0.15), (x0_ + dx*L_ + px_*ww_*0.15, y0_ + dy*L_ + py_*ww_*0.15)]
        d.polygon(pts, fill=255)
    im = im.resize((Wd//K, Hd//K), Image.LANCZOS)
    a = np.asarray(im).astype(np.float32)/255
    h_, w_ = a.shape
    x0p, y0p = P(cx) - w_//2, P(cy) - h_//2
    reg = img[y0p:y0p+h_, x0p:x0p+w_].astype(np.float32)
    col = np.array(color_rgb[::-1], np.float32)
    # leggero rilievo e un filo di usura come la serigrafia del pannello
    wear = np.clip(fbm(h_, w_, ((1.2,0.6),(3,0.4)), 77) + 2.2, 0, 1)
    a2 = a * (0.9 + 0.1*wear)
    img[y0p:y0p+h_, x0p:x0p+w_] = np.clip(reg*(1-a2[...,None]) + col*a2[...,None], 0, 255).astype(np.uint8)
comet_icon(126, 209, 34, (38, 128, 214))
fill_rect(280, 792, 291, 810, 14)    # tacca bianca accanto

# EQ: le 4 manopole (gonna con indice = frequenza, cappuccio = guadagno)
EQX, EQY = 225.0, [325.0, 465.0, 605.0, 745.0]
API_SRC = img.copy()
for yb in EQY:
    # tolgo l'indice "a torre" dal fondo (lo disegna la manopola ruotata)
    x0s, y0s, x1s, y1s = P(EQX-18), P(yb-66), P(EQX+18), P(yb-26)
    reg = img[y0s:y1s, x0s:x1s]
    yy, xx = np.mgrid[0:reg.shape[0], 0:reg.shape[1]]
    disc = np.hypot(xx - (P(EQX)-x0s), yy - (P(yb)-y0s)) < P(31.5)
    tower = (np.abs(xx - (P(EQX)-x0s)) < P(14)) & ~disc
    hsv = cv2.cvtColor(reg, cv2.COLOR_BGR2HSV)
    panel = reg[(~tower) & (~disc) & (lum(reg) < 60) & (hsv[...,1] < 90)]
    col = np.median(panel, axis=0) if len(panel) > 30 else np.array([22,22,24], np.float32)
    g = fbm(reg.shape[0], reg.shape[1], ((0.6*S,0.6),(2*S,0.4)), int(yb)) * 3.0
    mt = cv2.GaussianBlur(cv2.dilate(tower.astype(np.uint8), np.ones((3,3),np.uint8), iterations=P(0.6)).astype(np.float32), (0,0), P(0.6)) * (~disc)
    fillc = col[None,None,:] + g[...,None]
    img[y0s:y1s, x0s:x1s] = np.clip(reg*(1-mt[...,None]) + fillc*mt[...,None], 0, 255).astype(np.uint8)

# ======================================================================= 3. LA-2A
# BLEND via, scritte vecchie via
def plate_fill_disc(cx, cy, r, donor_box, seed):
    """riempie un cerchio con il tono del pannello intorno + la texture spazzolata vera"""
    global img
    x0, y0, x1, y1 = P(cx-r-10), P(cy-r-10), P(cx+r+10), P(cy+r+10)
    reg = img[y0:y1, x0:x1].astype(np.float32)
    hh, ww = reg.shape[:2]
    m = disc_mask(cx, cy, r, x0, y0, (hh, ww)).astype(np.float32)
    m = cv2.GaussianBlur(m, (0,0), P(2.0))
    keep = 1 - np.clip(m*1.5, 0, 1)
    low = None
    for kk in (1, 3, 8):
        sg = P(6)*kk
        den = cv2.GaussianBlur(keep, (0,0), sg)
        cur = cv2.GaussianBlur(reg*keep[...,None], (0,0), sg) / (den[...,None] + 1e-6)
        if low is None: low = cur; wsum = np.clip(den/0.2, 0, 1)
        else: low = low*wsum[...,None] + cur*(1-wsum[...,None]); wsum = np.maximum(wsum, np.clip(den/0.2, 0, 1))
    a, b, c, d = donor_box
    don = img[P(b):P(d), P(a):P(c)].astype(np.float32)
    hfd = don - cv2.GaussianBlur(don, (0,0), P(3))
    while hfd.shape[0] < hh: hfd = np.concatenate([hfd, hfd[::-1]], axis=0)
    while hfd.shape[1] < ww: hfd = np.concatenate([hfd, hfd[:, ::-1]], axis=1)
    hfd = hfd[:hh, :ww]
    img[y0:y1, x0:x1] = np.clip(reg*(1-m[...,None]) + (low + hfd)*m[...,None], 0, 255).astype(np.uint8)
plate_fill_disc(490, 846, 56, (640, 795, 720, 895), 21)
fill_rect(440, 783, 545, 812, 22)
for b in [(468,507,535,533),(578,507,622,533),(536,574,560,597),(627,576,646,595),(466,636,526,664)]:
    fill_rect(*b, 23+b[0]%7, pad=4)
# pomello di destra: rimpicciolito (diventa OUTPUT)
RK = (1110.0, 725.0)
OUTK = (1110.0, 725.0); OUTSC = 1.0   # INPUT grande quanto PEAK REDUCTION: resta il pomello originale
if OUTSC < 0.999:
    knob_src = img[P(RK[1]-84):P(RK[1]+84), P(RK[0]-84):P(RK[0]+84)].copy()
    plate_fill_disc(RK[0], RK[1], 120, (990, 470, 1215, 600), 24)
    D = int(round(knob_src.shape[0]*OUTSC))
    ks = cv2.resize(knob_src.astype(np.float32), (D, D), interpolation=cv2.INTER_AREA)
    m = np.zeros((D, D), np.float32); cv2.circle(m, (D//2, D//2), int(D*0.485), 1.0, -1)
    m = cv2.GaussianBlur(m, (0,0), P(0.8))
    sh = np.zeros((D+P(20), D+P(20)), np.float32); cv2.circle(sh, ((D+P(20))//2+P(2), (D+P(20))//2+P(4)), int(D*0.42), 1.0, -1)
    sh = cv2.GaussianBlur(sh, (0,0), P(4))
    gx, gy = P(OUTK[0]) - (D+P(20))//2, P(OUTK[1]) - (D+P(20))//2
    img[gy:gy+D+P(20), gx:gx+D+P(20)] = (img[gy:gy+D+P(20), gx:gx+D+P(20)].astype(np.float32)*(1-0.45*sh[...,None])).astype(np.uint8)
    paste(ks, m, OUTK[0], OUTK[1])
# indici dei pomelli LA-2A via (bianchi)
def erase_white_in_disc(cx, cy, r, thr=120):
    global img
    R = P(r); x0, y0 = P(cx)-R, P(cy)-R
    reg = img[y0:y0+2*R, x0:x0+2*R]
    L = lum(reg); yy, xx = np.mgrid[0:2*R, 0:2*R]
    disk = np.hypot(xx-R, yy-R) < R
    col = np.median(reg[disk & (L < 45)], axis=0)
    mk = cv2.dilate((disk & (L > thr)).astype(np.uint8), np.ones((3,3), np.uint8), iterations=P(1.0)) * disk
    out = reg.astype(np.float32); out[mk > 0] = col
    b = cv2.GaussianBlur(out, (0,0), P(0.9))
    md = cv2.dilate(mk, np.ones((3,3), np.uint8), iterations=2) > 0
    out[md] = b[md]
    img[y0:y0+2*R, x0:x0+2*R] = out.astype(np.uint8)
erase_white_in_disc(620, 725, 72)
erase_white_in_disc(OUTK[0], OUTK[1], 72*OUTSC)
# scale nuove 0-10 (le vecchie avevano numeri sbagliati)
def dial_band(cx, cy, r_num, half, values, size, ink, seed, mode='dark'):
    R = r_num + half + 6
    x0, y0 = P(cx-R), P(cy-R)
    reg = img[y0:P(cy+R), x0:P(cx+R)]
    yy, xx = np.mgrid[0:reg.shape[0], 0:reg.shape[1]]
    d = np.hypot(xx - (P(cx)-x0), yy - (P(cy)-y0))
    band = ((d > P(r_num-half)) & (d < P(r_num+half))).astype(np.float32)
    L = lum(reg)
    good = (L < 90) if mode == 'light' else (L > 110)
    fill_region(x0, y0, P(cx+R), P(cy+R), cv2.GaussianBlur(band, (0,0), P(1.0)), sigma=4, seed=seed,
                keep_extra=good.astype(np.float32))
    items = []
    for i, v in enumerate(values):
        a = np.radians(-135 + 270*i/(len(values)-1) - 90)
        items.append((cx + np.cos(a)*r_num, cy + np.sin(a)*r_num + 0.6, str(v), 'sansb', size, 0))
    paint_text(items, ink, seed+1, mode='silk', wear=0.45)
LAINK = (26, 28, 34)
dial_band(620, 725, 101, 13, list(range(11)), 17, LAINK, 30)
dial_band(OUTK[0], OUTK[1], 101, 16, list(range(11)), 17, LAINK, 32)
# etichette
paint_text([(493,524,'IN / OFF','sansb',16,0.6)], LAINK, 40, wear=0.5)
paint_text([(604,524,'LIMIT / COMP','sansb',16,0.4)], LAINK, 42, wear=0.5)
paint_text([(620,852,'INPUT','sansb',18,1.2)], LAINK, 44, wear=0.5)
paint_text([(OUTK[0],852,'PEAK REDUCTION','sansb',18,1.2)], LAINK, 45, wear=0.5)
# VU nuovo
FX0, FY0, FX1, FY1 = 716, 546, 975, 650
VU_PIVOT = ((FX0+FX1)/2, FY1 + 36)
VU_R = 110.0; VU_SPAN = 46.0; VK = (FX1-FX0)/182.0
def vu_pos(db): return (10**(db/20) - 0.1) / (10**(3/20) - 0.1)
def vu_ang(db): return -VU_SPAN + 2*VU_SPAN*vu_pos(db)
face = make_vu_face(); fh, fw = face.shape[:2]
x0, y0 = P(FX0), P(FY0)
mk = np.zeros((fh, fw), np.float32); cv2.rectangle(mk, (P(1),P(1)), (fw-P(1), fh-P(1)), 1.0, -1)
mk = cv2.GaussianBlur(mk, (0,0), P(0.7))[..., None]
img[y0:y0+fh, x0:x0+fw] = (face[..., ::-1].astype(np.float32)*mk + img[y0:y0+fh, x0:x0+fw].astype(np.float32)*(1-mk)).astype(np.uint8)
VU_FACE_BOX = (x0, y0, fw, fh)
# logo: lettere scure (si accendono col plugin)
plate_fill_disc(848, 732, 20, (640, 795, 720, 895), 50)
# logo dell'utente: disegno a linee scure su carta -> maschera delle linee
LG = cv2.imread(LOGO)
lgL = cv2.cvtColor(LG, cv2.COLOR_BGR2GRAY).astype(np.float32)
paper = cv2.GaussianBlur(lgL, (0,0), 25)
lines = np.clip((paper - lgL - 25) / 90.0, 0, 1)
ys, xs = np.where(lines > 0.5)
pad0 = 10
lines = lines[max(0, ys.min()-pad0):ys.max()+pad0, max(0, xs.min()-pad0):xs.max()+pad0]
LOGO_SIZE = 118.0                       # larghezza in pixel logici
sc = LOGO_SIZE*S / lines.shape[1]
letters = cv2.resize(lines, (int(lines.shape[1]*sc), int(lines.shape[0]*sc)), interpolation=cv2.INTER_AREA)
letters = np.clip(letters*1.25, 0, 1)
lcx, lcy = letters.shape[1]/2, letters.shape[0]/2
LOGO_C = (848.0, 785.0)
ox, oy = int(round(P(LOGO_C[0]) - lcx)), int(round(P(LOGO_C[1]) - lcy)); lh, lw = letters.shape
LOGO_ALPHA = np.zeros((H, W), np.float32); LOGO_ALPHA[oy:oy+lh, ox:ox+lw] = letters
bx0, by0, bx1, by1 = ox-P(6), oy-P(6), ox+lw+P(6), oy+lh+P(6)
reg = img[by0:by1, bx0:bx1].astype(np.float32); a = LOGO_ALPHA[by0:by1, bx0:bx1]; hh, ww = a.shape
a = cv2.GaussianBlur(a, (0,0), 0.35*S)
M_ = np.float32([[1,0,0],[0,1,0.6*S]]); hl = cv2.GaussianBlur(cv2.warpAffine(a, M_, (ww, hh)), (0,0), 0.5*S)
reg = reg + (np.clip(hl - a, 0, 1)*50)[..., None]
inkc = np.array([24,22,22], np.float32)
img[by0:by1, bx0:bx1] = np.clip(reg*(1-a[...,None]) + inkc*a[...,None], 0, 255).astype(np.uint8)

# ======================================================================= 4. VOG + alimentatore
VW = (222, 224, 226)
# scale 0-10 per amplitude e frequency (numeri sbagliati nella foto)
dial_band(535, 335, 89, 10, list(range(11)), 14.5, VW, 60, mode='light')
dial_band(780, 335, 89, 10, list(range(11)), 14.5, VW, 62, mode='light')
# LED: diventano il selettore CENTER 40 / 200
fill_rect(898, 258, 925, 278, 64, pad=3); fill_rect(896, 302, 925, 322, 65, pad=3)
paint_text([(907,268,'40','poppins',15,0)], VW, 66, wear=0.4)
paint_text([(905,312,'200','poppins',15,0)], VW, 67, wear=0.4)
paint_text([(940,232,'center','poppins',14,0.3)], VW, 68, wear=0.4)
for (x, y, tint) in [(940, 268, (70,20,16)), (940, 312, (20,46,22))]:
    m = np.zeros((H, W), np.uint8); cv2.circle(m, (P(x), P(y)), P(14), 255, -1)
    img = cv2.inpaint(img, m, P(3), cv2.INPAINT_TELEA)
    R_ = P(16); x0l, y0l = P(x)-R_, P(y)-R_
    reg = img[y0l:y0l+2*R_, x0l:x0l+2*R_]
    img[y0l:y0l+2*R_, x0l:x0l+2*R_] = draw_led(reg, R_, R_, P(8.5), tint[::-1] if False else (tint[2], tint[1], tint[0]), False).astype(np.uint8)
# etichette dei pulsantini
for i, (x, t) in enumerate([(925,'in'), (985,'hp'), (1045,'24dB'), (1105,'ø')]):
    paint_text([(x, 409, t, 'poppins', 14, 0.3)], VW, 70+i, wear=0.4)
# Q e OUTPUT: copie piccole di "amplitude" (con il loro arco di tacche)
copy_knob(535, 335, 82, 1200, 316, 0.46)
erase_white_in_disc(1200, 316, 26)
paint_text([(1200,262,'q','poppins',20,0)], VW, 80, wear=0.4)
# alimentatore: ON sul secondo tasto, MASTER al posto del bilanciere
paint_text([(1435,243,'ON','sans',22,0.5)], VW, 82, wear=0.35)
fill_rect(1290, 300, 1468, 370, 83, sigma=4, pad=6)
OUTF = (1312.0, 862.0)
copy_knob(535, 335, 82, OUTF[0], OUTF[1], 0.46)
erase_white_in_disc(OUTF[0], OUTF[1], 25.5)
paint_text([(1395,853,'OUTPUT','sans',17,1.2)], VW, 84, wear=0.3)
paint_text([(1395,872,'final','poppins',11,0.6)], VW, 85, wear=0.3)
CLIP = (1450.0, 862.0)
R_ = P(16); x0l, y0l = P(CLIP[0])-R_, P(CLIP[1])-R_
reg = img[y0l:y0l+2*R_, x0l:x0l+2*R_]
img[y0l:y0l+2*R_, x0l:x0l+2*R_] = draw_led(reg, R_, R_, P(5.0), (16,20,70), False).astype(np.uint8)
# indici bianchi dei pomelli VOG (li disegna il plugin)
erase_white_in_disc(535, 335, 50)
erase_white_in_disc(780, 335, 50)

# ======================================================================= 5. sprite
BASE = img.copy()
sprites = {}
def Y(v): return v - CROP_Y
def save_sprite(name, bgr, alpha, cx, cy, extra=None):
    rgba = np.dstack([bgr[..., ::-1], np.clip(alpha*255, 0, 255)]).astype(np.uint8)
    Image.fromarray(rgba, 'RGBA').save(f'{OUT}/{name}.png')
    sprites[name] = {'x': float(cx), 'y': float(Y(cy)), 'w': bgr.shape[1]/S, 'h': bgr.shape[0]/S}
    if extra: sprites[name].update(extra)

def lit_led_sprite(name, cx, cy, r, color):
    R_ = P(r*3); x0, y0 = P(cx)-R_, P(cy)-R_
    reg = BASE[y0:y0+2*R_, x0:x0+2*R_].astype(np.float32)
    out = draw_led(reg, R_, R_, P(r), color[::-1], True)
    a = soft_disc(2*R_, 2*R_, R_*0.95, P(2))
    save_sprite(name, out, a, cx, cy)
lit_led_sprite('led_40_on', 940, 268, 8.5, (255,40,30))
lit_led_sprite('led_200_on', 940, 312, 8.5, (60,255,70))
lit_led_sprite('led_ts_on', LEDX, LEDY, 4.6, (255,40,30))
lit_led_sprite('led_clip_on', CLIP[0], CLIP[1], 5.0, (255,40,30))
# LED EQ IN (rosso nella foto): base spenta + sprite acceso dalla foto
x0e, y0e = P(162)-P(16), P(810)-P(16)
save_sprite('led_eq_on', ORIG[y0e:y0e+P(32), x0e:x0e+P(32)].astype(np.float32), soft_disc(P(32), P(32), P(14), P(2)), 162, 810)
m = np.zeros((H, W), np.uint8); cv2.circle(m, (P(162), P(810)), P(13), 255, -1)
BASE = cv2.inpaint(BASE, m, P(3), cv2.INPAINT_TELEA)
reg = BASE[y0e:y0e+P(32), x0e:x0e+P(32)]
BASE[y0e:y0e+P(32), x0e:x0e+P(32)] = draw_led(reg, P(16), P(16), P(10), (16,20,70), False).astype(np.uint8)

def glow_sprite(name, cx, cy, r, color, radius, inner=0.85, halo=0.35):
    x0, y0 = P(cx)-P(r), P(cy)-P(r)
    c = BASE[y0:y0+2*P(r), x0:x0+2*P(r)].astype(np.float32)
    yy, xx = np.mgrid[0:c.shape[0], 0:c.shape[1]].astype(np.float32)
    d = np.hypot(xx - c.shape[1]/2, yy - c.shape[0]/2) / S
    g = np.clip(1 - d/radius, 0, 1)**1.4
    hlo = np.clip(1 - d/(r*0.95), 0, 1)**2 * halo
    o = np.clip(c + (g[...,None]*inner + hlo[...,None]) * np.array(color[::-1], np.float32) * 0.9, 0, 255)
    save_sprite(name, o, soft_disc(c.shape[0], c.shape[1], c.shape[0]/2*0.92, P(1.5)), cx, cy)
for i, x in enumerate([925, 985, 1045, 1105]):
    glow_sprite(f'btn_on_{i}', x, 375, 30, (255,150,60), 15)
# tasto ON acceso
x0p, y0p, x1p, y1p = P(1400), P(212), P(1470), P(272)
c = BASE[y0p:y1p, x0p:x1p].astype(np.float32)
yy, xx = np.mgrid[0:c.shape[0], 0:c.shape[1]].astype(np.float32)
core = np.exp(-(((xx-c.shape[1]/2)/(c.shape[1]*0.32))**2 + ((yy-c.shape[0]/2)/(c.shape[0]*0.30))**2))
inside = np.zeros(c.shape[:2], np.float32); cv2.rectangle(inside, (P(6),P(6)), (c.shape[1]-P(6), c.shape[0]-P(6)), 1.0, -1)
light = cv2.GaussianBlur(inside, (0,0), P(2)) * (0.3 + 0.7*core)
o = np.clip(c*(1+light[...,None]*1.2) + light[...,None]*np.array([30,60,255], np.float32)*0.5, 0, 255)
save_sprite('pow_on', o, cv2.GaussianBlur(inside*0+1, (0,0), 1), (1400+1470)/2, (212+272)/2)
# levette LA-2A (su / giù), metallo come i dadi della foto
def lever(up_):
    Wd, Hd = P(34), P(56)
    im = Image.new('RGBA', (Wd, Hd), (0,0,0,0)); d = ImageDraw.Draw(im)
    cx, cy = Wd/2, Hd/2; sgn = -1 if up_ else 1; Ln = P(22)
    shl = Image.new('L', (Wd, Hd), 0)
    ImageDraw.Draw(shl).polygon([(cx-P(3)+P(2), cy+P(2)), (cx+P(3)+P(2), cy+P(2)), (cx+P(4.4)+P(3), cy+sgn*Ln+P(4)), (cx-P(4.4)+P(3), cy+sgn*Ln+P(4))], fill=160)
    shl = shl.filter(ImageFilter.GaussianBlur(P(1.6)))
    im.paste(Image.new('RGBA', (Wd, Hd), (6,5,4,255)), (0,0), shl)
    for i in range(-P(4), P(4)+1):
        t = (i + P(4)) / (2*P(4))
        v = 80 + 140*np.exp(-((t-0.38)/0.22)**2)
        d.line([(cx + i*0.7, cy), (cx + i, cy + sgn*Ln)], fill=(int(v), int(v*0.98), int(v*0.95), 255), width=1)
    d.ellipse((cx-P(4.6), cy+sgn*Ln-P(2.6), cx+P(4.6), cy+sgn*Ln+P(2.6)), fill=(170,168,162,255))
    d.ellipse((cx-P(3), cy+sgn*Ln-P(1.8), cx+P(1.2), cy+sgn*Ln+P(0.4)), fill=(230,228,222,255))
    return im.filter(ImageFilter.GaussianBlur(0.8))
for nm, upf in [('lever_up', True), ('lever_down', False)]:
    lv = lever(upf); lv.save(f'{OUT}/{nm}.png')
    sprites[nm] = {'x': 0.0, 'y': 0.0, 'w': lv.width/S, 'h': lv.height/S}
# VU spento
x0v, y0v, fw, fh = VU_FACE_BOX
f = BASE[y0v:y0v+fh, x0v:x0v+fw].astype(np.float32); lumf = f.mean(axis=2, keepdims=True)
off = (f*0.3 + lumf*0.7) * np.array([0.22, 0.30, 0.40])[None, None, :]
a = np.zeros((fh, fw), np.float32); cv2.rectangle(a, (P(1),P(1)), (fw-P(1), fh-P(1)), 1.0, -1)
save_sprite('vu_off', off, cv2.GaussianBlur(a, (0,0), P(0.8)), FX0+(FX1-FX0)/2, FY0+(FY1-FY0)/2)
# logo acceso: ambra leggibile
pad = P(20); ys, xs = np.where(LOGO_ALPHA > 0.02)
lx0, ly0, lx1, ly1 = xs.min()-pad, ys.min()-pad, xs.max()+pad, ys.max()+pad
G = LOGO_ALPHA[ly0:ly1, lx0:lx1]; k3 = np.ones((3,3), np.uint8)
halo1 = cv2.GaussianBlur(G, (0,0), 1.6*S); halo2 = cv2.GaussianBlur(G, (0,0), 6*S)
core = np.clip(G*1.15, 0, 1)
rgb = np.ones(G.shape+(3,), np.float32)*np.array([40,150,255], np.float32)          # alone ambra (BGR)
rgb = rgb*(1-core[...,None]) + np.array([120,205,255], np.float32)*core[...,None]   # linee ambra chiaro
hot = cv2.GaussianBlur(cv2.erode(G, k3, iterations=1), (0,0), 0.5)
rgb = rgb*(1-hot[...,None]*0.5) + np.array([200,240,255], np.float32)*hot[...,None]*0.5
alpha = np.clip(core + halo1*0.55 + halo2*0.30, 0, 1)
save_sprite('logo_glow', rgb, alpha, (lx0+lx1)/2/S, (ly0+ly1)/2/S)
# EQ: manopole in 7 posizioni (frequenza) con la luce ferma
for bi, yb in enumerate(EQY):
    R = 66
    x0, y0 = P(EQX)-P(R), P(yb)-P(R)
    c = API_SRC[y0:y0+2*P(R), x0:x0+2*P(R)].astype(np.float32); Dk = c.shape[0]
    sig = 4.0*S; low0 = cv2.GaussianBlur(c, (0,0), sig)
    frames = []
    for kf in range(7):
        ang = -135 + 45*kf
        Mr = cv2.getRotationMatrix2D((Dk/2-0.5, Dk/2-0.5), -ang, 1.0)
        rot = cv2.warpAffine(c, Mr, (Dk, Dk), flags=cv2.INTER_CUBIC, borderMode=cv2.BORDER_REFLECT)
        fixed = np.clip(rot - cv2.GaussianBlur(rot, (0,0), sig) + low0, 0, 255)
        a = soft_disc(Dk, Dk, P(31.5), 1.5)
        tw = np.zeros((Dk, Dk), np.float32)
        pts = np.array([[Dk/2-P(13), Dk/2-P(28)], [Dk/2+P(13), Dk/2-P(28)], [Dk/2+P(4), Dk/2-P(64)], [Dk/2-P(4), Dk/2-P(64)]], np.int32)
        cv2.fillPoly(tw, [pts], 1.0)
        tw = cv2.warpAffine(tw, Mr, (Dk, Dk))
        a = np.maximum(a, cv2.GaussianBlur(tw, (0,0), 1.2))
        frames.append(np.dstack([fixed[..., ::-1], a*255]).astype(np.uint8))
    Image.fromarray(np.concatenate(frames, axis=1), 'RGBA').save(f'{OUT}/api_skirt_{bi}.png')
    sprites[f'api_skirt_{bi}'] = {'x': EQX, 'y': float(Y(yb)), 'w': Dk/S, 'h': Dk/S, 'frames': 7}

# pomelli del compressore che girano davvero: texture del pomello (con il suo indice) + luce fissa
# runtime: pixel = texture ruotata - luce ruotata + luce fissa  (i riflessi restano fermi, il pomello gira)
for nm, (kx, ky) in [('knob_in', (620.0, 725.0)), ('knob_pr', (1110.0, 725.0))]:
    R = 84
    y0k, y1k, x0k, x1k = P(ky-R), P(ky+R), P(kx-R), P(kx+R)
    tex = ORIG[y0k:y1k, x0k:x1k].astype(np.float32)                 # pomello originale con l'indice bianco
    light = cv2.GaussianBlur(BASE[y0k:y1k, x0k:x1k].astype(np.float32), (0,0), 6*S)   # luce senza indice
    ones = np.ones(tex.shape[:2], np.float32)
    save_sprite(nm, tex, ones, kx, ky, extra={'disc': 79.0})
    save_sprite(nm + '_light', light, ones, kx, ky)

FINAL = BASE[P(CROP_Y):, :]
cv2.imwrite(f'{OUT}/background.jpg', FINAL, [cv2.IMWRITE_JPEG_QUALITY, 91])
cv2.imwrite(f'{OUT}/_final.png', FINAL)
controls = {
    'knobs': {
        'b1f': [EQX, Y(EQY[0])], 'b2f': [EQX, Y(EQY[1])], 'b3f': [EQX, Y(EQY[2])], 'b4f': [EQX, Y(EQY[3])],
        'laGain': [620, Y(725)], 'laPeak': [OUTK[0], Y(OUTK[1])],
        'vogAmp': [535, Y(335)], 'vogFreq': [780, Y(335)], 'vogQ': [1200, Y(316)],
        'master': [OUTF[0], Y(OUTF[1])],
        **{k_: [v[0], Y(v[1])] for k_, v in TS_KNOBS.items()},
    },
    'footswitch': [FS[0], Y(FS[1]), FS[2]-FS[0], FS[3]-FS[1]],
    'tsLed': [LEDX, Y(LEDY)],
    'clipLed': [CLIP[0], Y(CLIP[1])],
}
json.dump({'size': [W0, H0-CROP_Y], 'scale': S, 'sprites': sprites, 'controls': controls,
           'vu': {'pivot': [VU_PIVOT[0], Y(VU_PIVOT[1])], 'radius': VU_R, 'span': VU_SPAN,
                  'face': [FX0, Y(FY0), FX1-FX0, FY1-FY0]}},
          open(f'{OUT}/layout.json', 'w'), indent=1)
print('done', len(sprites), FINAL.shape)
