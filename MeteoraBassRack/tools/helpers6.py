def fbm(h, w, scales=((3,0.5),(9,0.3),(24,0.2)), seed=0):
    r = np.random.default_rng(seed); out = np.zeros((h, w), np.float32)
    for s, a in scales:
        n = r.standard_normal((h, w)).astype(np.float32)
        n = cv2.GaussianBlur(n, (0,0), s); n /= (n.std() + 1e-6); out += a*n
    return out / (out.std() + 1e-6)


def erase_black_pointer(cx, cy, r, p0, p1, w=4.5):
    yy, xx = np.mgrid[0:H, 0:W]
    disk = np.hypot(xx-P(cx), yy-P(cy)) < P(r)*0.98
    L = lum(img)
    col = np.median(img[disk & (L < 50)], axis=0)
    m = np.zeros((H, W), np.uint8)
    cv2.line(m, (P(p0[0]), P(p0[1])), (P(p1[0]), P(p1[1])), 255, P(w), cv2.LINE_AA)
    m = (m > 0) & disk
    img[m] = col
    b = cv2.GaussianBlur(img, (0,0), 1.4)
    md = cv2.dilate(m.astype(np.uint8), np.ones((5,5), np.uint8)) > 0
    img[md] = b[md]


def render_text(h, w, items):
    """items: (x, y, text, font, size, tracking) in coordinate locali 2x -> alpha 0..1"""
    im = Image.new('L', (w, h), 0); d = ImageDraw.Draw(im)
    for (x, y, t, f, size, track) in items:
        font = ImageFont.truetype(FONTS[f], int(round(size*S)))
        ws = [d.textlength(c, font=font) for c in t]
        tot = sum(ws) + track*S*(len(t)-1)
        cx = x - tot/2
        for c, cw in zip(t, ws):
            d.text((cx, y), c, font=font, fill=255, anchor='lm'); cx += cw + track*S
    return np.asarray(im).astype(np.float32) / 255.0


def silkscreen(box, mode, replace=None, ink=None, strength=1.0, seed=0, extra_mask=None, clean=True):
    """Invecchia (o rifà) una scritta serigrafata.
       box: (x0,y0,x1,y1) unità 1x; mode: 'dark' inchiostro scuro su chiaro, 'light' chiaro su scuro
       replace: lista di (x,y,testo,font,size,tracking) in unità 1x assolute, oppure None per tenere la scritta"""
    global img
    m = 6
    x0, y0, x1, y1 = P(box[0])-P(m), P(box[1])-P(m), P(box[2])+P(m), P(box[3])+P(m)
    reg = img[y0:y1, x0:x1].astype(np.float32)
    L = lum(reg.astype(np.uint8))
    bgL = cv2.medianBlur(L.astype(np.uint8), 2*P(6)+1).astype(np.float32)
    diff = (bgL - L) if mode == 'dark' else (L - bgL)
    inner = np.zeros_like(L); inner[P(m):-P(m), P(m):-P(m)] = 1
    a0 = np.clip((diff - 6) / 22.0, 0, 1) * inner
    if extra_mask is not None: a0 = np.maximum(a0, extra_mask)
    hard = (a0 > 0.12).astype(np.uint8)
    hard = cv2.dilate(hard, np.ones((3,3), np.uint8), iterations=2)
    bg = cv2.inpaint(reg.astype(np.uint8), hard*255, 4, cv2.INPAINT_TELEA).astype(np.float32) if clean else reg.copy()
    if ink is None:
        sel = a0 > 0.75
        ink = np.median(reg[sel], axis=0) if sel.sum() > 10 else np.array([40,40,40], np.float32)
        if mode == 'dark': ink = ink * 0.85
    ink = np.array(ink, np.float32)
    if replace is not None:
        items = [((x*S - x0), (y*S - y0), t, f, sz, tr) for (x, y, t, f, sz, tr) in replace]
        a = render_text(L.shape[0], L.shape[1], items)
    else:
        a = a0
    hh, ww = a.shape
    # bordi irregolari (l'inchiostro non è mai perfetto)
    dx = fbm(hh, ww, ((1.2,0.7),(3,0.3)), seed+1) * 0.45
    dy = fbm(hh, ww, ((1.2,0.7),(3,0.3)), seed+2) * 0.45
    gx, gy = np.meshgrid(np.arange(ww, dtype=np.float32), np.arange(hh, dtype=np.float32))
    a = cv2.remap(a.astype(np.float32), gx+dx, gy+dy, cv2.INTER_LINEAR)
    # usura: scheggiature, densità irregolare, sporco del pannello che copre l'inchiostro
    n = fbm(hh, ww, ((1.5,0.5),(4,0.35),(10,0.15)), seed+3)
    chips = np.clip((n + 2.15) * 2.2, 0, 1)
    dens = 0.88 + 0.12 * np.clip(fbm(hh, ww, ((14,1.0),), seed+4), -1, 1)
    bgc = cv2.GaussianBlur(bg, (0,0), P(1.5))
    ref = np.median(bgc.reshape(-1,3), axis=0)
    dev = np.abs(bgc - ref).mean(axis=2)
    grime = np.clip((dev - 12) / 45.0, 0, 1)
    a = np.clip(a * 1.08, 0, 1) * (1 - (1-chips)*0.75*strength) * dens * (1 - 0.3*grime*strength)
    a = cv2.GaussianBlur(a, (0,0), 0.7)
    # l'inchiostro prende la grana del pannello
    bgLn = lum(bg.astype(np.uint8)); tex = bgLn / (bgLn.mean() + 1e-3)
    inkc = ink[None, None, :] * (0.86 + 0.14*tex[..., None])
    hf = bg - cv2.GaussianBlur(bg, (0,0), 1.6)
    out = bg*(1-a[...,None]) + inkc*a[...,None] + hf*0.55*a[...,None]
    img[y0:y1, x0:x1] = np.clip(out, 0, 255).astype(np.uint8)


def fill_region(x0, y0, x1, y1, m, sigma=6, seed=0, keep_extra=None):
    global img
    reg = img[y0:y1, x0:x1].astype(np.float32)
    keep = 1 - np.clip(m*1.5, 0, 1)
    if keep_extra is not None: keep = keep * keep_extra
    sg = P(sigma); low = None
    for k in (1, 4, 12):
        den = cv2.GaussianBlur(keep, (0,0), sg*k)
        cur = cv2.GaussianBlur(reg*keep[...,None], (0,0), sg*k) / (den[...,None] + 1e-6)
        if low is None: low = cur; wsum = np.clip(den/0.2, 0, 1)
        else:
            low = low*wsum[...,None] + cur*(1-wsum[...,None]); wsum = np.maximum(wsum, np.clip(den/0.2, 0, 1))
    hf = reg - cv2.GaussianBlur(reg, (0,0), P(2.5)/2)
    sel = keep > 0.9
    std = hf[sel].std(axis=0) if sel.sum() > 50 else np.array([4,4,4], np.float32)
    std = np.minimum(std, 5.0)
    hh, ww = m.shape
    g1 = fbm(hh, ww, ((0.6*S,0.5),(1.5*S,0.35),(4*S,0.15)), seed+700)
    g2 = fbm(hh, ww, ((0.6*S,0.5),(1.5*S,0.35),(4*S,0.15)), seed+701)
    grain = np.dstack([g1*std[0], (0.7*g1+0.3*g2)*std[1], (0.5*g1+0.5*g2)*std[2]])
    mm = np.clip(m, 0, 1)[..., None]
    img[y0:y1, x0:x1] = np.clip(reg*(1-mm) + (low + grain)*mm, 0, 255).astype(np.uint8)

def paint_text(items, ink, seed, mode='silk', wear=1.0, shadow=0.0, hilite=0.0):
    """items: (x, y, testo, font, size, tracking) in unità 1x.
       mode 'silk' = serigrafia, 'engraved' = incisione riempita di vernice (anni '60/'70)"""
    global img
    xs = [it[0] for it in items]; ys = [it[1] for it in items]
    pad = 26
    padx = max(pad*2.5, max(len(it[2])*it[4]*0.45 + 20 for it in items))
    x0, y0 = P(min(xs)-padx), P(min(ys)-pad)
    x1, y1 = P(max(xs)+padx), P(max(ys)+pad)
    reg = img[y0:y1, x0:x1].astype(np.float32)
    hh, ww = reg.shape[:2]
    loc = [((x*S - x0), (y*S - y0), t, f, sz, tr) for (x, y, t, f, sz, tr) in items]
    a = render_text(hh, ww, loc)
    dx = fbm(hh, ww, ((1.2,0.7),(3,0.3)), seed+1) * 0.5
    dy = fbm(hh, ww, ((1.2,0.7),(3,0.3)), seed+2) * 0.5
    gx, gy = np.meshgrid(np.arange(ww, dtype=np.float32), np.arange(hh, dtype=np.float32))
    a = cv2.remap(a, gx+dx, gy+dy, cv2.INTER_LINEAR)
    if shadow > 0:      # bordo dell'incisione / ombra dell'inchiostro
        M = np.float32([[1,0,0.45*S],[0,1,0.65*S]])
        sh = cv2.GaussianBlur(cv2.warpAffine(a, M, (ww, hh)), (0,0), 0.55*S)
        reg = reg * (1 - shadow*sh[..., None])
    if hilite > 0:      # luce sul bordo inferiore dell'incisione
        M = np.float32([[1,0,0],[0,1,-0.55*S]])
        hl = cv2.GaussianBlur(np.clip(a - cv2.warpAffine(a, M, (ww, hh)), 0, 1), (0,0), 0.4*S)
        reg = reg + hilite*hl[..., None]*255*0.35
    n = fbm(hh, ww, ((1.5,0.5),(4,0.35),(10,0.15)), seed+3)
    thr = 2.15 if mode == 'silk' else 1.75
    chips = np.clip((n + thr) * 2.2, 0, 1)
    dens = 0.88 + 0.12*np.clip(fbm(hh, ww, ((14,1.0),), seed+4), -1, 1)
    bgc = cv2.GaussianBlur(reg, (0,0), P(1.5))
    ref = np.median(bgc.reshape(-1,3), axis=0)
    grime = np.clip((np.abs(bgc - ref).mean(axis=2) - 12) / 45.0, 0, 1)
    a = a * (1 - (1-chips)*0.8*wear) * dens * (1 - 0.3*grime*wear)
    a = cv2.GaussianBlur(a, (0,0), 0.35*S)
    bgLn = reg.mean(axis=2); tex = bgLn / (bgLn.mean() + 1e-3)
    inkc = np.array(ink, np.float32)[None, None, :] * (0.88 + 0.12*tex[..., None])
    hf = reg - cv2.GaussianBlur(reg, (0,0), 0.8*S)
    out = reg*(1-a[...,None]) + inkc*a[...,None] + hf*0.5*a[...,None]
    img[y0:y1, x0:x1] = np.clip(out, 0, 255).astype(np.uint8)


def erase(box, mode, thr=9, dil=1.2):
    global img
    m = 4
    x0, y0, x1, y1 = P(box[0])-P(m), P(box[1])-P(m), P(box[2])+P(m), P(box[3])+P(m)
    reg = img[y0:y1, x0:x1]
    L = lum(reg); bgL = cv2.medianBlur(L.astype(np.uint8), 2*P(6)+1).astype(np.float32)
    inner = np.zeros_like(L); inner[P(m):-P(m), P(m):-P(m)] = 1
    t = (np.abs(L - bgL) > thr).astype(np.uint8) * inner.astype(np.uint8)
    t = cv2.dilate(t, np.ones((3,3), np.uint8), iterations=P(dil)).astype(np.float32)
    t = cv2.GaussianBlur(t, (0,0), P(0.8))
    fill_region(x0, y0, x1, y1, np.clip(t*1.4, 0, 1), sigma=4, seed=int(box[0]))


def dial(cx, cy, r_in, r_out, r_num, values, size, ink, seed, angles=None, half=9.5):
    global img
    R = P(r_out) + P(4)
    x0, y0 = P(cx)-R, P(cy)-R
    reg = img[y0:y0+2*R, x0:x0+2*R]
    L = lum(reg); bgL = cv2.medianBlur(L.astype(np.uint8), 2*P(5)+1).astype(np.float32)
    yy, xx = np.mgrid[0:2*R, 0:2*R]
    d = np.hypot(xx-R, yy-R)
    ann = (d > P(r_in)) & (d < P(r_out))
    band = ((d > P(r_num-half)) & (d < P(r_num+half))).astype(np.float32)
    band = cv2.GaussianBlur(band, (0,0), P(1.2))
    # escludo dal riempimento i pixel scuri (tacche, pomello) così il tono viene dal pannello
    bright = (L > np.percentile(L[band > 0.5], 40)).astype(np.float32)
    fill_region(x0, y0, x0+2*R, y0+2*R, band, sigma=5, seed=int(cx), keep_extra=bright)
    items = []
    if angles is None:
        angles = [-135 + 270*i/(len(values)-1) for i in range(len(values))]
    for v, a in zip(values, angles):
        t = np.radians(a - 90)
        items.append((cx + np.cos(t)*r_num, cy + np.sin(t)*r_num + 0.5, str(v), 'sansb', size, 0))
    xs = [it[0] for it in items]; ys = [it[1] for it in items]
    silkscreen((min(xs)-12, min(ys)-9, max(xs)+12, max(ys)+9), 'dark', replace=items, ink=ink, seed=seed, clean=False)


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t*t*(3 - 2*t)


def make_vu_face():
    K = 4                                   # super-sampling
    w, h = int((FX1-FX0)*S*K/2*2), int((FY1-FY0)*S*K/2*2)
    sc = S*K
    sk = sc*VK
    px, py = (VU_PIVOT[0]-FX0)*sc, (VU_PIVOT[1]-FY0)*sc
    # carta retroilluminata
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    dc = np.hypot((xx - w*0.5)/(w*0.62), (yy - h*0.62)/(h*0.95))
    glow = np.clip(1 - dc, 0, 1)**0.9
    c_hot  = np.array([255, 184, 70], np.float32)
    c_edge = np.array([200, 86, 14], np.float32)
    face = c_edge + (c_hot - c_edge) * glow[..., None]
    img_f = Image.fromarray(np.clip(face, 0, 255).astype(np.uint8)).convert('RGBA')
    d = ImageDraw.Draw(img_f)
    ink = (34, 22, 14, 255); red = (168, 32, 20, 255)
    R = VU_R*sc
    def pt(a, r): t = np.radians(a-90); return (px + np.cos(t)*r, py + np.sin(t)*r)
    def arc(a0, a1, r, wdt, col):
        steps = 120
        pts = [pt(a0 + (a1-a0)*i/steps, r) for i in range(steps+1)]
        d.line(pts, fill=col, width=int(wdt), joint='curve')
    # scala principale
    arc(vu_ang(-20), vu_ang(0), R, 1.1*sk, ink)
    arc(vu_ang(0), vu_ang(3), R + 1.6*sk, 4.2*sk, red)
    arc(vu_ang(0), vu_ang(3), R, 1.1*sk, red)
    # tacche
    for db in np.concatenate([np.arange(-20, -10, 1.0), np.arange(-10, -3, 0.5), np.arange(-3, 3.01, 0.25)]):
        major = abs(db - round(db)) < 1e-6 and (db in (-20,-10,-7,-5,-3,-2,-1,0,1,2,3))
        if not major and db < -10 and db % 2 != 0: continue
        a = vu_ang(db); col = red if db > 0 else ink
        l = 7*sk if major else 3.4*sk
        d.line([pt(a, R), pt(a, R + l)], fill=col, width=int((1.25 if major else 0.8)*sk))
    fnum = ImageFont.truetype(FONTS['sansb'], int(9.6*sk))
    for db, t in [(-20,'20'),(-10,'10'),(-7,'7'),(-5,'5'),(-3,'3'),(-2,'2'),(-1,'1'),(0,'0'),(1,'1'),(2,'2'),(3,'3')]:
        a = vu_ang(db); col = red if db > 0 else ink
        x, y = pt(a, R + 15.5*sk)
        d.text((x, y), t, font=fnum, fill=col, anchor='mm')
    # scala in percentuale (sotto l'arco, piccola)
    fsm = ImageFont.truetype(FONTS['sans'], int(6.2*sk))
    for pc in [0, 20, 40, 60, 80, 100]:
        db = 20*np.log10(0.1 + pc/100*(10**(3/20)-0.1)) if pc > 0 else -20
        a = vu_ang(db)
        d.line([pt(a, R - 1.5*sk), pt(a, R - 4.5*sk)], fill=(34,22,14,200), width=int(0.8*sk))
        x, y = pt(a, R - 10.5*sk); d.text((x, y), str(pc), font=fsm, fill=(34,22,14,190), anchor='mm')
    fvu = ImageFont.truetype(FONTS['sansb'], int(13*sk))
    d.text((px, (FY1-FY0)*sc*0.80), 'VU', font=fvu, fill=ink, anchor='mm')
    fpm = ImageFont.truetype(FONTS['sansb'], int(12*sk))
    d.text((9*sk, 11*sk), '–', font=fpm, fill=ink, anchor='mm')
    d.text(((FX1-FX0)*sc - 9*sk, 11*sk), '+', font=fpm, fill=red, anchor='mm')
    fbrand = ImageFont.truetype(FONTS['sans'], int(5.2*sk))
    d.text((px, (FY1-FY0)*sc*0.93), 'GAIN REDUCTION', font=fbrand, fill=(34,22,14,170), anchor='mm')
    # riduco a 2x
    face2 = img_f.resize((int(round((FX1-FX0)*S)), int(round((FY1-FY0)*S))), Image.LANCZOS)
    f = np.asarray(face2.convert('RGB')).astype(np.float32)
    hh, ww = f.shape[:2]
    # carta: grana, fibre, macchie, ingiallimento verso i bordi
    f *= (1 + 0.035*fbm(hh, ww, ((0.8,0.6),(2.5,0.4)), 71)[..., None])
    f *= (1 - 0.04*np.clip(fbm(hh, ww, ((10,1.0),), 72), 0, None)[..., None])
    yy, xx = np.mgrid[0:hh, 0:ww].astype(np.float32)
    vig = np.clip(np.hypot((xx-ww/2)/(ww*0.56), (yy-hh*0.55)/(hh*0.75)), 0, 1.2)
    f *= (1 - 0.22*vig**3)[..., None]
    # ombra della cornice in alto e ai lati
    top = np.exp(-yy/(hh*0.10)); side = np.exp(-xx/(ww*0.03)) + np.exp(-(ww-1-xx)/(ww*0.03))
    f *= (1 - 0.45*np.clip(top + side, 0, 1))[..., None]
    f = cv2.GaussianBlur(f, (0,0), 0.7)
    # vetro: riflesso diagonale leggero
    gl = np.clip(1 - np.abs((xx/ww)*0.8 + (yy/hh)*0.6 - 0.42)/0.10, 0, 1) * np.clip(1 - yy/(hh*0.8), 0, 1)
    f = f + gl[..., None]*14
    return np.clip(f, 0, 255).astype(np.uint8)


def save_rgba(name, bgr, alpha, cx, cy, extra=None):
    rgba = np.dstack([bgr[..., ::-1], np.clip(alpha*255, 0, 255)]).astype(np.uint8)
    Image.fromarray(rgba, 'RGBA').save(f'{OUT}/{name}.png')
    sprites[name] = {'x': float(cx), 'y': float(cy), 'w': bgr.shape[1]/S, 'h': bgr.shape[0]/S}
    if extra: sprites[name].update(extra)


def box(cx, cy, rx, ry=None):
    ry = rx if ry is None else ry
    return P(cx)-P(rx), P(cy)-P(ry), P(cx)+P(rx), P(cy)+P(ry)

def soft_disc(h, w, r, feather):
    m = np.zeros((h, w), np.float32); cv2.circle(m, (w//2, h//2), int(r), 1.0, -1)
    return cv2.GaussianBlur(m, (0,0), feather)


def lit_from_orig(name, cx, cy, r):
    x0, y0, x1, y1 = box(cx, cy, r)
    o = ORIG[y0:y1, x0:x1].astype(np.float32)
    a = soft_disc(y1-y0, x1-x0, P(r)*0.8, P(r)*0.12)
    save_rgba(name, o, a, cx, cy)
    return o - BASE[y0:y1, x0:x1].astype(np.float32)


def lit_from_delta(name, cx, cy, delta, color=None, gain=1.0):
    h, w = delta.shape[:2]
    x0, y0 = P(cx) - w//2, P(cy) - h//2
    b = BASE[y0:y0+h, x0:x0+w].astype(np.float32)
    dl = np.clip(delta, 0, None)
    if color is not None:
        k = dl.max(axis=2, keepdims=True) / 255.0
        dl = k * np.array(color[::-1], np.float32)[None, None, :] * gain
    o = np.clip(b + dl, 0, 255)
    a = soft_disc(h, w, min(h, w)/2*0.8, min(h, w)*0.06)
    save_rgba(name, o, a, cx, cy)


def toggle_down(name, cx, cy, rx=14, ry=24, lw=4.5, ll=19, nut=12.5, thr=120):
    x0, y0, x1, y1 = box(cx, cy, rx, ry)
    c = BASE[y0:y1, x0:x1].copy()
    Lc = lum(c); h, w = Lc.shape
    yy, xx = np.mgrid[0:h, 0:w]
    lever = (Lc > thr) & (np.abs(xx - (P(cx)-x0)) < P(lw)) & (yy < (P(cy)-y0) + P(1)) & (yy > (P(cy)-y0) - P(ll))
    lever = cv2.dilate(lever.astype(np.uint8), np.ones((3,3),np.uint8), iterations=1)
    lev_rgb = c.copy()
    # leva capovolta attorno al centro del dado
    fl = cv2.flip(lev_rgb, 0); fm = cv2.flip(lever, 0).astype(np.float32)
    shift = int(round(2*(P(cy) - y0) - h))
    M = np.float32([[1,0,0],[0,1,shift]])
    fl = cv2.warpAffine(fl, M, (w, h)); fm = cv2.warpAffine(fm, M, (w, h))
    # dove c'era la leva rimetto il dado: metà inferiore specchiata (il dado è rotondo)
    ld = cv2.dilate(lever, np.ones((3,3),np.uint8), iterations=2).astype(np.float32)
    ld = cv2.GaussianBlur(ld, (0,0), 1.0)[..., None]
    mirror = cv2.warpAffine(cv2.flip(c, 0), M, (w, h), borderMode=cv2.BORDER_REPLICATE).astype(np.float32)
    mirror = mirror * 1.08   # la parte alta del dado prende un po' più luce
    rr = np.hypot(xx - (P(cx)-x0), yy - (P(cy)-y0))
    outside = ((ld[...,0] > 0.05) & (rr > P(nut))).astype(np.uint8)*255
    base_in = cv2.inpaint(c, cv2.dilate(outside, np.ones((3,3),np.uint8), iterations=1), 4, cv2.INPAINT_TELEA).astype(np.float32)
    inside = cv2.GaussianBlur(((rr <= P(nut))).astype(np.float32), (0,0), 0.8)[..., None]
    clean = base_in*(1-ld) + (mirror*inside + base_in*(1-inside))*ld
    fm = cv2.GaussianBlur(fm, (0,0), 0.8)[..., None]
    # ombra della leva verso il basso
    sh = cv2.GaussianBlur(cv2.warpAffine(fm[...,0], np.float32([[1,0,P(1.2)],[0,1,P(1.6)]]), (w, h)), (0,0), P(1.5))[..., None]
    out = clean*(1 - 0.45*sh)
    out = out*(1-fm) + fl.astype(np.float32)*fm*0.92
    a = np.zeros((h, w), np.float32); cv2.ellipse(a, (w//2, h//2), (int(w*0.46), int(h*0.47)), 0, 0, 360, 1.0, -1)
    a = cv2.GaussianBlur(a, (0,0), P(1.2))
    save_rgba(name, out, a, cx, cy)

def inpaint_mask(mask, radius=5):
    global img
    img = cv2.inpaint(img, mask, radius, cv2.INPAINT_TELEA)


def copy_patch(src_c, dst_c, r, feather=2.5, src=None):
    """copia un dettaglio (es. pulsante spento) sopra un altro, con bordo morbido"""
    global img
    src = img if src is None else src
    R = P(r)
    a = src[P(src_c[1])-R:P(src_c[1])+R, P(src_c[0])-R:P(src_c[0])+R].astype(np.float32)
    mk = np.zeros((2*R, 2*R), np.float32); cv2.circle(mk, (R, R), int(R*0.82), 1.0, -1)
    mk = cv2.GaussianBlur(mk, (0,0), P(feather))[..., None]
    y0, x0 = P(dst_c[1])-R, P(dst_c[0])-R
    d = img[y0:y0+2*R, x0:x0+2*R].astype(np.float32)
    img[y0:y0+2*R, x0:x0+2*R] = (a*mk + d*(1-mk)).astype(np.uint8)


def calm_glints(cx, cy, r0, r1):
    global img
    R = P(r1)
    x0, y0 = P(cx)-R, P(cy)-R
    reg = img[y0:y0+2*R, x0:x0+2*R]
    hsv = cv2.cvtColor(reg, cv2.COLOR_BGR2HSV).astype(np.float32)
    yy, xx = np.mgrid[0:2*R, 0:2*R]; d = np.hypot(xx-R, yy-R)
    ann = ((d > P(r0)) & (d < P(r1))).astype(np.float32)
    ann = cv2.GaussianBlur(ann, (0,0), P(3))
    ref_s = np.median(hsv[...,1][ann > 0.5])
    excess = np.clip(hsv[...,1] - ref_s - 15, 0, None)
    hsv[...,1] -= excess * 0.75 * ann
    img[y0:y0+2*R, x0:x0+2*R] = cv2.cvtColor(np.clip(hsv,0,255).astype(np.uint8), cv2.COLOR_HSV2BGR)
