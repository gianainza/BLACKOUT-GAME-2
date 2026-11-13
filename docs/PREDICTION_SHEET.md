# R2 - PREDICTION SHEET

**What I am about to predict:**
How closely the CPU lit-test (`LitTest.cpp`) and the GPU shadow lookup (`shader.frag` debugMode 2) will agree on 20 sample positions.

---

## PREDICTION
*written on 2026-11-8*

**What I think will happen:**
The CPU and GPU will agree exactly on positions that are clearly in the light or clearly in the dark. They will disagree on about 5 to 10 of the 20 positions, but ONLY the ones sampled near a shadow edge. The CPU will mostly say "LIT" where the GPU says "SHADOW" because of the bias margin.

**My reasoning, in full:**
There are three reasons they cannot agree perfectly:
1. **CUBE MAP QUANTISATION:** The GPU samples a discrete cube map. At d=6 and R=1024, one texel covers 2*6/1024 = 0.0117 world units. The CPU computes the exact ray. Near an edge, the GPU can hit the wrong texel.
2. **BIAS:** The GPU adds omniBias (0.025 world units) before comparing depth. This shifts the shadow boundary outward. The CPU has no bias. So right next to an edge, the GPU will call it shadow sooner than the CPU.
3. **SAMPLE POINT MISMATCH:** The CPU samples at the capsule foot (y = SAMPLE_HEIGHT = 0.01). The GPU probe samples the floor fragment directly under the player. At a grazing angle, these are physically different points.

**What would have to be true for me to be wrong:**
If they disagree FAR from an edge (where distance to edge > 0.1), that means I have a bug. It would mean the cube map, the far plane, or the CPU geometry is wrong -- not just quantisation.

---

## MEASUREMENT
*taken on 2026-11-9*

**What actually happened:**
7 out of 20 samples disagreed. 4 of the disagreements were on the edge samples (rows 2, 4, 7, 10), where the CPU and GPU disagreed on exactly ONE light's visibility (the mask differed by 1 bit). However, 3 of the disagreements happened FAR from an edge (rows 11, 14, 17), where the masks were identical, but the CPU said LIT and the GPU said SHADOW.

**Was the prediction right:** partly

**If not: what had I misunderstood, and what do I now believe instead:**
I predicted that disagreements would ONLY happen near an edge due to quantisation and bias. I was partly wrong. The edge disagreements were exactly as predicted. But I missed a third reason: the GPU applies a 20-sample PCF blur (diskRadius). Even far from a shadow, this blur averages the shadow factor slightly below 1.0 (e.g., 0.8 instead of 1.0), which lowers the overall `gpu_illum`. This causes the GPU to drop below the lit threshold (0.15) on a few samples where the CPU (which has no blur) stays above it.