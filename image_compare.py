#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pathlib import Path
import math
import cv2 as cv
import numpy as np
import time
import csv


# -------------------- Konfiguration --------------------

LABEL_TOP_LENGTH_CM = 9.7  # <--- Hier anpassen, falls anderer Eingabeordner
INPUT_DIR = Path("2025-09-29")
INPUT_DIR.mkdir(parents=True, exist_ok=True)
SAVE_OVERLAY = True
OUT_DIR = Path("out")
OUT_DIR.mkdir(parents=True, exist_ok=True)

# -------------------- Hilfsfunktionen --------------------


def _order_box_points(pts: np.ndarray) -> np.ndarray:
    s = pts.sum(axis=1)
    diff = np.diff(pts, axis=1).reshape(-1)
    tl = pts[np.argmin(s)]
    br = pts[np.argmax(s)]
    tr = pts[np.argmin(diff)]
    bl = pts[np.argmax(diff)]
    return np.array([tl, tr, br, bl], dtype=np.float32)


def _largest_label_mask(gray: np.ndarray) -> np.ndarray:
    blur = cv.GaussianBlur(gray, (5, 5), 0)
    _, th = cv.threshold(blur, 0, 255, cv.THRESH_BINARY + cv.THRESH_OTSU)
    mask = th if np.count_nonzero(th) < 0.5 * th.size else cv.bitwise_not(th)
    kernel = cv.getStructuringElement(cv.MORPH_RECT, (3, 3))
    mask = cv.morphologyEx(mask, cv.MORPH_CLOSE, kernel, iterations=1)
    mask = cv.morphologyEx(mask, cv.MORPH_OPEN, kernel, iterations=1)
    contours, _ = cv.findContours(
        mask, cv.RETR_EXTERNAL, cv.CHAIN_APPROX_SIMPLE)
    if not contours:
        raise RuntimeError("Kein Label gefunden.")
    cnt = max(contours, key=cv.contourArea)
    label_mask = np.zeros_like(mask)
    cv.drawContours(label_mask, [cnt], -1, 255, thickness=cv.FILLED)
    return label_mask, cnt


def _fit_top_edge_line(gray_roi: np.ndarray):
    h, w = gray_roi.shape
    band_h = max(10, int(0.30 * h))
    band = gray_roi[:band_h, :]
    band_blur = cv.GaussianBlur(band, (3, 3), 0)
    dy = cv.Sobel(band_blur, cv.CV_64F, 0, 1, ksize=3)
    dy_pos = np.maximum(dy, 0)
    y_idx = dy_pos.argmax(axis=0)
    val = dy_pos.max(axis=0)
    thr = 0.3 * np.percentile(val, 95) if np.any(val > 0) else 0
    valid_mask = val > thr
    xs = np.arange(w, dtype=np.float32)[valid_mask]
    ys = y_idx[valid_mask].astype(np.float32)

    if xs.size < max(20, w * 0.3):
        edges = cv.Canny(band_blur, 50, 150, apertureSize=3, L2gradient=True)
        lines = cv.HoughLinesP(
            edges, 1, np.pi/180, threshold=50, minLineLength=w*0.5, maxLineGap=10)
        if lines is None:
            raise RuntimeError("Kantenfit fehlgeschlagen.")
        line = max(lines[:, 0, :], key=lambda L: abs(L[0]-L[2]))
        x1, y1, x2, y2 = map(float, line)
        angle = math.degrees(math.atan2(y2 - y1, x2 - x1))
        ptL = np.array([0.0, y1 + (0 - x1) * (y2 - y1) /
                       (x2 - x1 + 1e-6)], dtype=np.float32)
        ptR = np.array([w - 1.0, y1 + (w - 1 - x1) *
                       (y2 - y1) / (x2 - x1 + 1e-6)], dtype=np.float32)
        return ptL, ptR, angle

    pts = np.vstack([xs, ys]).T.astype(np.float32).reshape(-1, 1, 2)
    [vx, vy, x0, y0] = cv.fitLine(pts, cv.DIST_L2, 0, 0.01, 0.01)
    vx, vy, x0, y0 = float(vx), float(vy), float(x0), float(y0)

    def y_at(x):
        if abs(vx) < 1e-6:
            return y0
        t = (x - x0) / vx
        return y0 + t * vy

    xL, xR = 0.0, float(w - 1)
    yL, yR = float(y_at(xL)), float(y_at(xR))
    angle = math.degrees(math.atan2(vy, vx))
    ptL = np.array([xL, yL], dtype=np.float32)
    ptR = np.array([xR, yR], dtype=np.float32)
    return ptL, ptR, angle

# -------------------- Anpassung: robuste Erkennung auch bei abgeschnittenem Label --------------------


def detect_reference_line(image_bgr: np.ndarray):
    """
    REFERENZLINIE direkt aus einem oberen Band des axis-aligned Bounding-Rects
    der größten Label-Komponente schätzen. Funktioniert auch, wenn das Label
    nicht vollständig im Bild ist (z. B. unten abgeschnitten).
    """
    img = image_bgr
    gray = cv.cvtColor(img, cv.COLOR_BGR2GRAY)

    mask, cnt = _largest_label_mask(gray)

    # Axis-aligned Bounding-Box des (ggf. abgeschnittenen) Labels
    x, y, w, h = cv.boundingRect(cnt)

    # Oberes Analyseband: etwas oberhalb und unterhalb der erwarteten Kante zulassen
    margin_top = 6
    # genügend Kontext, auch bei abgeschnittenem Label
    band_extra = min(int(0.4 * h) + 20, h)
    y0 = max(0, y - margin_top)
    y1 = min(gray.shape[0] - 1, y + band_extra)
    x0 = max(0, x)
    x1 = min(gray.shape[1] - 1, x + w)

    roi = gray[y0:y1 + 1, x0:x1 + 1]
    if roi.size == 0:
        raise RuntimeError("ROI leer.")

    # Präzise obere Kante im ROI
    ptL_roi, ptR_roi, _ = _fit_top_edge_line(roi)

    # Punkte in Bildkoordinaten
    tl = ptL_roi + np.array([x0, y0], dtype=np.float32)
    tr = ptR_roi + np.array([x0, y0], dtype=np.float32)

    # Winkel, Länge, Skala
    top_vec = tr - tl
    angle_deg = math.degrees(math.atan2(top_vec[1], top_vec[0]))
    edge_len_px = float(np.linalg.norm(top_vec))
    if edge_len_px <= 1:
        raise RuntimeError("Obere Kante degeneriert.")
    px_per_cm = edge_len_px / LABEL_TOP_LENGTH_CM
    center_edge = (tl + tr) / 2.0

    if SAVE_OVERLAY:
        canvas = image_bgr.copy()
        # nur als grobe Orientierung
        cv.rectangle(canvas, (x, y), (x + w, y + h), (0, 255, 0), 2)
        cv.line(canvas, tuple(tl.astype(int)),
                tuple(tr.astype(int)), (255, 0, 0), 2)
        cv.circle(canvas, tuple(center_edge.astype(int)), 4, (0, 0, 255), -1)
        cv.putText(canvas, f"Angle: {angle_deg:+.2f} deg", (10, 30),
                   cv.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)
        overlay = canvas
    else:
        overlay = None

    return {
        "tl": tl.astype(np.float32),
        "tr": tr.astype(np.float32),
        "angle_deg": float(angle_deg),
        "edge_len_px": edge_len_px,
        "px_per_cm": float(px_per_cm),
        "center": center_edge.astype(np.float32),
        "overlay": overlay,
    }

# -------------------- Vergleich & Ausgabe (unverändert seit letzter Version) --------------------


def compare_two_images(img_path_ref: Path, img_path_cur: Path):
    ref_img = cv.imread(str(img_path_ref), cv.IMREAD_COLOR)
    cur_img = cv.imread(str(img_path_cur), cv.IMREAD_COLOR)
    if ref_img is None or cur_img is None:
        raise FileNotFoundError("Bilddatei(en) nicht gefunden.")

    res_ref = detect_reference_line(ref_img)
    res_cur = detect_reference_line(cur_img)

    cm_per_px = 1.0 / res_ref["px_per_cm"]
    mm_per_px = 10.0 * cm_per_px

    u = res_ref["tr"] - res_ref["tl"]
    u = u / np.linalg.norm(u)

    n_candidate = np.array([-u[1], u[0]], dtype=np.float32)
    up_dir = np.array([0.0, -1.0], dtype=np.float32)
    n = n_candidate if np.dot(n_candidate, up_dir) >= 0 else -n_candidate

    d_center_px = float(np.dot(res_cur["center"] - res_ref["center"], n))
    d_center_mm = d_center_px * mm_per_px

    delta_angle = float(res_cur["angle_deg"] - res_ref["angle_deg"])
    while delta_angle >= 180.0:
        delta_angle -= 360.0
    while delta_angle < -180.0:
        delta_angle += 360.0

    d_left_px = float(np.dot(res_cur["tl"] - res_ref["tl"], n))
    d_right_px = float(np.dot(res_cur["tr"] - res_ref["tr"], n))
    d_left_mm = d_left_px * mm_per_px
    d_right_mm = d_right_px * mm_per_px

    # Analysebilder mit Präfix "analyse_" und Originalnamen speichern
    if SAVE_OVERLAY:
        cv.imwrite(
            str(OUT_DIR / f"analyse_{img_path_ref.name}"), res_ref["overlay"])
        cv.imwrite(
            str(OUT_DIR / f"analyse_{img_path_cur.name}"), res_cur["overlay"])

    return {
        "offset_center_px": d_center_px,
        "offset_center_mm": d_center_mm,
        "rotation_delta_deg": delta_angle,
        "left_offset_px": d_left_px,
        "left_offset_mm": d_left_mm,
        "right_offset_px": d_right_px,
        "right_offset_mm": d_right_mm,
        # Absolute Eckpunkte (aktuelles Bild / B)
        "cur_tl_abs": tuple(map(float, res_cur["tl"])),
        "cur_tr_abs": tuple(map(float, res_cur["tr"])),
        # Umrechnungsfaktor: 1 px in cm (aus Referenzbild)
        "cm_per_px": cm_per_px
    }


# --- CSV-Ausgabe auf Semikolon umgestellt (nur diesen Teil ersetzen) ---

if __name__ == "__main__":
    exts = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"}
    files = sorted([p for p in INPUT_DIR.iterdir()
                   if p.suffix.lower() in exts])

    start = time.time()

    header = [
        "Vergleich/Metriken",
        "Orthogonaler Abstand (B relativ zu A)",
        "Rotationsdifferenz",
        "Linke Ecke (B relativ zu A)",
        "Rechte Ecke (B relativ zu A)",
        "linker eckpunkt absolut",
        "rechter eckpunkt absolut",
        "1px in cm",
    ]
    rows = []

    # Alle Bilder mit dem ersten Bild vergleichen (statt fortlaufend Bild i zu Bild i+1)
    if len(files) >= 2:
        ref = files[0]

        def pair(a, b):
            return f"{a.name}  ->  {b.name}"

        def fmt_mm_px(mm, px):
            try:
                return f"{mm:.6f} mm | {px:.6f} px"
            except Exception:
                return "-"

        def fmt_deg(deg):
            try:
                return f"{deg:.6f} °"
            except Exception:
                return "-"

        def fmt_pt(pt):
            try:
                x, y = pt
                return f"({x:.2f}, {y:.2f})"
            except Exception:
                return "-"

        def fmt_factor(v):
            try:
                return f"{v:.8f}"
            except Exception:
                return "-"

        for i in range(1, len(files)):
            img_a, img_b = ref, files[i]
            stats = compare_two_images(img_a, img_b)
            rows.append([
                pair(img_a, img_b),
                fmt_mm_px(stats.get("offset_center_mm"),
                          stats.get("offset_center_px")),
                fmt_deg(stats.get("rotation_delta_deg")),
                fmt_mm_px(stats.get("left_offset_mm"),
                          stats.get("left_offset_px")),
                fmt_mm_px(stats.get("right_offset_mm"),
                          stats.get("right_offset_px")),
                fmt_pt(stats.get("cur_tl_abs")),
                fmt_pt(stats.get("cur_tr_abs")),
                fmt_factor(stats.get("cm_per_px")),
            ])

    csv_path = OUT_DIR / "vergleichsergebnisse.csv"
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f, delimiter=";", quotechar='"',
                            quoting=csv.QUOTE_MINIMAL, lineterminator="\n")
        writer.writerow(header)
        writer.writerows(rows)

    elapsed = time.time() - start
    print(f"Anzahl Bilder: {len(files)}")
    print(f"Gesamtdauer [s]: {elapsed:.3f}")
