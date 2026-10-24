# DEV CONSOLE TUTORIAL
Format komendy: {prefiks}_{nazwa} {argumenty}

Przyklad:
sp_speed 1.25
log_fps 1
wrd_timescale 0.5
r_bloom 0
cam_fov 100

---

# DEV CONSOLE COMMANDS & PREFIXES

## #SP (SUPER / CHEATS & GRACZ)
Wymaga wczesniejszego wpisania `sp_cheats 1`.

sp_:
cheats 0/1                  (odblokowuje komendy deweloperskie i cheaty)
god 0/1                     (niesmiertelnosc gracza)
noclip 0/1                  (latanie i swobodne przenikanie przez sciany)
bhop 0/1                    (wlaczenie/wylaczenie fizyki zachowania pedu bunny hop)
speed {wartosc}             (mnoznik predkosci gracza, np. 1.0 = domyslna, 1.5 = szybszy)
jump {wartosc}              (sila/wysokosc skoku gracza)
get {id}                    (dodanie broni po krotkim ID, np. 101, 120, 312)
heal {wartosc}              (leczenie lub ustawienie zdrowia gracza)
armor {wartosc}             (dodanie/ustawienie punktow pancerza)
ammo {wartosc}              (uzupelnienie amunicji w biezacej lub wszystkich broniach)
give_all                    (dodaje wszystkie bronie i pelna amunicje)
respawn                     (natychmiastowe odrodzenie gracza na punkcie startowym)
suicide                     (samobojstwo gracza do testow respawnu i ragdolla)


## #LOG (OVERLAY 2D NA EKRANIE - CONST TELEMETRY)
log_:
momentum 0/1                (pokazuje aktualna predkosc u/s, ped i wektor ruchu a'la cl_showpos)
fps 0/1                     (licznik klatek na sekunde oraz frametime w ms)
ping 0/1                    (opoznienie sieciowe pakietow w ms)
pos 0/1                     (wspolrzedne gracza X, Y, Z oraz katy patrzenia pitch/yaw)
colliders 0/1               (rysowanie obrysow AABB kolizji w swiecie 3D)
triggers 0/1                (wizualizacja obrysow stref Trigger Volumes)
ai 0/1                      (wyswietlanie stanu AI, aktualnego celu i punktu trasy)
drawcalls 0/1               (liczba wywolan renderowania draw calls i wierzcholkow na klatke)
memory 0/1                  (zuzycie RAM i VRAM przez tekstury/siatki)
all 0/1                     (wlacza lub wylacza zbiorczo wszystkie overlaye)


## #WRD (ŚWIAT, MAPA, ŚRODOWISKO LOKALNE)
wrd_:
map {nazwa}                 (zaladowanie wskazanej sceny/mapy, np. site14_corridor)
restart                     (zresetowanie biezacej mapy do stanu poczatkowego)
skybox {wartosc}            (zmiana tla nieba: HEX koloru np. #1a1a24 lub nazwa cubemapy)
model {id/sciezka}          (postawienie wybranego modelu 3D w miejscu celownika)
timescale {wartosc}         (mnoznik predkosci uplywu czasu silnika: 0.5 slow-mo, 1.0 normalnie, 2.0 szybko)
wireframe 0/1               (przelaczenie widoku sceny na siatke poligonow)
fog 0/1                     (wlaczenie/wylaczenie mgly atmosferycznej w korytarzu)
fog_density {wartosc}       (gestosc mgly, np. 0.02)
light_ambient {hex/rgb}     (ogolna jasnosc swiatla otoczenia w scenie)


## #CAM (KAMERA, VIEWMODEL & FOV)
cam_:
fov {stopnie}               (glowny kat widzenia kamery, np. 90, 105, 110)
viewmodel_fov {stopnie}     (kat widzenia modelu broni w rekach)
viewmodel_offset {x y z}    (pozycja trzymania broni na ekranie)
thirdperson 0/1             (przelaczenie miedzy widokiem FPP a kamera zza plecow TPP)
bob 0/1                     (kolysanie kamery i broni podczas biegu)
shake {sila}                (wywolanie testowego wstrzasu kamery, np. wybuch)
freecam 0/1                 (odpiecie kamery od ciala gracza - swobodny lot i inspekcja sceny)


## #R (RENDERER, SHADERY & GRAFIKA NISKOPOZIOMOWA)
r_:
bloom 0/1                   (wlaczenie/wylaczenie subtelnego efektu bloom)
fxaa 0/1                    (wlaczenie/wylaczenie antyaliasingu FXAA 3.11)
vsync 0/1                   (synchronizacja pionowa z monitorem)
shadows 0/1                 (dynamiczne cienie z map cieni)
shadow_res {512/1024/2048}  (rozdzielczosc tekstury cieni)
lightmaps 0/1               (wypalane oswietlenie statyczne)
normalmaps 0/1              (mapowanie normalnych na powierzchniach)
cubemaps 0/1                (odbicia otoczenia z cubemap)
postprocess 0/1             (zbiorcze wylaczenie calego post-processingu)
reload_shaders              (przeladowanie wszystkich plikow shaderow GLSL bez restartu)


## #PHYS (FIZYKA, GRAWITACJA & PARAMETRY BHOPA)
phys_:
gravity {wartosc}           (przyspieszenie grawitacyjne, domyslnie -9.81)
friction {wartosc}          (tarcie podloza gracza, domyslnie 4.0 - mniejsze = wiekszy poslizg)
air_accel {wartosc}         (przyspieszenie w locie do strafe jumpingu, np. 10.0 - 100.0)
bhop_window {ms}            (czas w milisekundach na wykonanie idealnego bunny hopa bez utraty pedu)
step_height {wartosc}       (maksymalna wysokosc stopni/progów do plynnego wejscia)
ragdoll_force {mnoznik}     (sila odrzutu ciala po trafieniu/smierci)
debug 0/1                   (wizualne rysowanie wektorow predkosci i punktow kontaktu)


## #AI (SZTUCZNA INTELIGENCJA & STORY BOTS)
ai_:
disable 0/1                 (calkowite zatrzymanie logiki decyzyjnej AI)
freeze 0/1                  (zamrozenie ruchu botow w biezacej klatce)
sight 0/1                   (rysowanie stozka widzenia i zasiegu percepcji bota)
paths 0/1                   (rysowanie sciezki waypointow po ktorych porusza sie bot)
aggro 0/1                   (wymuszenie natychmiastowej wrogosci botow do gracza)
ignore_player 0/1           (boty ignoruja gracza i skupiaja sie wylacznie na scenariuszu)
spawn {typ}                 (postawienie bota danego typu w miejscu celownika)


## #SND (DŹWIĘK, AUDIO 3D & LIP-SYNC)
snd_:
volume {0.0-1.0}            (glosnosc glowna master)
music {0.0-1.0}             (glosnosc muzyki w tle)
sfx {0.0-1.0}               (glosnosc efektow dzwiekowych: strzaly, kroki, wybuchy)
voice {0.0-1.0}             (glosnosc kwestii dialogowych i mowy)
play {sciezka}              (natychmiastowe odtworzenie wskazanego pliku wav/ogg)
stop_all                    (natychmiastowe wyciszenie wszystkich kanalow dzwieku)
lipsync_test {plik}         (odtworzenie kwestii z jednoczesnym testem mimiki bota)
restart                     (zresetowanie urzadzenia audio OpenAL/DirectSound)


## #UI (INTERFEJS 2D, MENU & HUD)
ui_:
hide 0/1                    (calkowite ukrycie interfejsu i HUDa, idealne pod screeny)
crosshair 0/1               (wlaczenie/wylaczenie celownika)
crosshair_color {hex}       (kolor celownika, np. #00ff00 lub #ffffff)
scale {wartosc}             (mnoznik skali UI dla monitorow 1080p / 1440p / 4K)
menu {glowna/pauza/opcje}   (natychmiastowe wywolanie konkretnego ekranu 2D)
reload                      (przeladowanie layoutu aktywnego ekranu z pliku .labui)


## #DCL (DECALE, KREW, ŚLADY KUL & BRUD)
dcl_:
clear                       (usuniecie wszystkich plam i sladow z biezacej mapy)
limit {liczba}              (maksymalna dozwolona liczba decali jednoczesnie, np. 256)
spawn {typ}                 (postawienie decala w miejscu celownika: blood, bullet, dirt)
distance {metry}            (maksymalna odleglosc widzenia plam i zaciskow)


## #MP (MULTIPLAYER & SERWER)
mp_:
a_bots {liczba}             (liczba botow w druzynie A)
b_bots {liczba}             (liczba botow w druzynie B)
bot_kick_all                (usuniecie wszystkich botow multiplayer)
restart 1                   (restart rundy/meczu)
friendly_fire 0/1           (obrazenia pomiedzy graczami w tej samej druzynie)


## #NET (SIEĆ & DIAGNOSTYKA PAKIETÓW)
net_:
status                      (statystyki polaczenia: tickrate, pakiet loss, jitter)
fake_lag {ms}               (sztuczne opoznienie pakietow do testow lag compensation)
fake_loss {procent}         (sztuczne gubienie pakietow 0-100%)
connect {ip:port}           (polaczenie z dedykowanym serwerem)
disconnect                  (rozlaczenie z biezacym serwerem)


## #SYS (SYSTEM & KONSOLA)
sys_:
clear                       (wyczyszczenie historii tekstu w konsoli)
help {opcjonalny_prefiks}   (wypisanie listy dostepnych komend lub filtrowanie np. help r_)
bind {klawisz} {komenda}    (przypisanie klawisza do komendy konsoli, np. bind f5 "wrd_restart")
quit                        (natychmiastowe zamkniecie aplikacji silnika/gry)