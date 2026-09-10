# TODO & Roadmap – Lab Engine & Frozen-Life (FPS)

## I. NAJBLIZSZE SPRINTY (SHORT‑TERM / IN PROGRESS)

### 1. Rozbudowa Hammer Editora o Terrain Editor
- [ ] Dodanie trybu Terrain / Displacement Editing
- [ ] Narzędzia do rzeźbienia terenu (Raise/Lower, Smooth, Flatten, Noise)
- [ ] Painting tekstur na displacementach (multi-layer)

### 2. Seasonal Terrain Shader System
- [ ] Implementacja shader-based seasonal texture blending (albedo grass → snow)
- [ ] Dodanie parametru `_SnowCoverage` (0.0 – 1.0) sterującego przejściem
- [ ] Blend z kontrolowaną ostrością (`_BlendSharpness`) + height-based mask
- [ ] Wsparcie dla normal maps i smoothness/metallic w obu wariantach tekstur
- [ ] Integracja z Terrain Editor
- [ ] Runtime toggle sezonu + smooth transition (lerp w czasie)
- [ ] Testy wizualne: lato → zima, różne wysokości, kąty oświetlenia, LOD