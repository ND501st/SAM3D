# The Osa's function checks, receivers and drives: what the sources say (14 Nov 2026)

Research for the roadmap's C4 (function checks and faults), C5 (receivers and the radar's picture) and C6 (drives,
balances and the gyro), read from `voenizdat_osa_1991` (the extract's `=== PAGE n` is our printed page n-1; checked on
the page footers), `mon_2112_80`, `korovin_tiv_2010` and `samsim_osa`. The textbook describes the 9K33M3 (AKM): block
suffixes differ from the AK's (OP63-6M3 for OP63-6M2, OP81-16M2 for OP81-16M, the IFF 1Л24-2 for the AK's set), so a
detail is AK-true only with that in mind. Where a line says *inference*, it is SAM3D's reading, not the source's.

## C6: drives, balances, the gyro

- **ПА is rate-aided (high, p87-88, p73).** In ПА the АЗИМУТ (УГОЛ МЕСТА) wheel turns a tachogenerator in the βс (εс)
  mechanism; its voltage goes to the follow-up directly and through an integrator that controls the antenna by rate.
  The antenna carries on at the rate the wheel gave when the wheel stops (p264: in launch smoke the wheels stop and the
  crosshair is brought back smoothly when the target reappears). In ТСЦ the follow-ups track by position and rate, so
  ПА takes over at the target's rate (p73, p87). Limits 30°/s in azimuth, 15°/s in elevation (p85). ПА is used for
  helicopters, method φ, chaff, TV firing (p21, p130, p132, p264, p266).
- **БАЛАНС ПА (az, el):** not described anywhere; only the console photos show them. *Inference (medium):* they balance
  the ПА rate integrator; as push-buttons (the elevation one under a lid), pressing one zeroes the stored rate (low).
- **БАЛАНС АВТОМАТА (OS81-18M):** not in the textbook (SAM Simulator's label). The analogue on OK81-4M (p119) balances
  the range finder's time discriminators. *Inference (medium):* misset, the target's range gate sits off the echo's
  centre: a steady range bias.
- **СКОРОСТЬ П А:** only SAM Simulator's label; semi-automatic range tracking exists (p261, p264). Not modelled.
- **БАЛАНС (I), (II) on OK81-4M (high, p119):** balance the missile range finders' time discriminators; a setting-up
  adjustment, no combat use described.
- **ЭМУ ОТКЛ. — ВКЛ. (high, p99, p86):** cuts the control winding of the ЭМУ that drives the turret's motor: azimuth
  cannot move in any mode; elevation is unaffected. The ССЦ marker (βс) still follows the wheel (p86).
- **СС — ОТКЛ. on OR-31M (medium, p51, p41):** switches off the follow-up systems Ψ, Θ, Qc; their scales and the Qc
  that turns the PPI's sweep resolvers stop following.
- **0005-8M (p50-51):** ПИТАНИЕ СЕТЬ — ПТ (start СЕТЬ; ПТ not explained); РЕЛЕ — Ψ, Θ (switches off the ГА-3's horizontal
  correction; which position is off is not stated); ОБОГРЕВ (heats the ГА-3 below +5 °C); φ АВТ. — РУЧН. (connects ПУ-11's
  latitude correction to the ГА-3; it must be at РУЧН., p48); КОРРЕКЦИЯ ОСН. — КОНТР. (at ОСН. the latitude correction
  reaches the ГА-3; not on our model). Lamps ОТКАЗ Ψ, Θ and ОТКАЗ Qc.
- **The orientation drill (high, p247):** ends with ТСЦ then РУ and the КАНАЛ Ц meters' mismatch brought to no more than
  two divisions.
- Citation fixes: БАЛАНС (II) is p119 (was p120); МК — ГПК — АК is p51 (was p50).

## C5: receivers and the radar's picture

- **The MTI (high, p26-27, p36-40):** coherent on receive; a coherent heterodyne phased by the transmitter's pulse
  (internal coherence); a double canceller (0065-2M) shared by both radars (the СОЦ 0-35 km, the ССЦ a 1.5 km piece round
  its target); ground clutter left at most 5%; staggered periods T1/T2 or T1/T3 against blind speeds, chosen by the
  operator for the best target on the PPI, set automatically by the tracked target's radial speed after lock (p30); a
  fixed period against second-trace clutter. ШТ PRF 2800 Hz (p89). T1-T3 not given. MTI to 35 km, the plain channel 35-45
  km (p30).
- **External coherence (СОЦ only, p27):** the heterodyne is phased by the clutter itself: fixed and moving clutter
  cancel without wind compensation. The ССЦ has none (p72, p80).
- **РПЧМ — АПЧМ (high, p33-36, p50, p80, p98):** АПЧМ holds the magnetron at the local oscillator plus 30 MHz to ±0.5
  MHz, the tolerance the MTI's phasing can take; the magnetron drifts with its cavities' temperature (no rate given);
  РПЧМ holds the tuning stem where the РЕГ. РПЧМ potentiometer sets it (mid-travel when the frequency is set up); used
  for setting up, or in combat when АПЧМ fails. What the operator watches in РПЧМ: not stated. *Inference (medium):* off
  tune, the MTI's cancellation goes first, then the sensitivity.
- **Wind compensation (high, p36-37, p49, p53, p98, p100, p266):** in internal coherence the reference is shifted by the
  clutter's Doppler, set by hand with ГРУБО and ТОЧНО until the clutter is cancelled on the PPI (СОЦ) or the range
  indicator (ССЦ); the vehicle's own radial speed is added on the move. One offset, not by azimuth (*inference*). Kept
  up through guidance in chaff (p266).
- **СДЦ ВНЕШН. — ВНУТР. (high, p49, p265-266):** external or internal coherence for the СОЦ. In chaff the radars work in
  СДЦ; when chaff and ground clutter cannot both be cancelled, external coherence.
- **СА — ОТКЛ. (high, p30, p36, p266; mon_2112_80 p67):** blanks non-synchronous pulse interference from neighbouring
  radars; used against non-synchronous repeater jamming.
- **0051-7M (high, p98, p30, p53, p266):** PRF switch АВТ., РУЧН. F1—F2, РУЧН. F1—F3, F3, F1 (not on our model).
- **0065-2M (p40, p50):** АРУ — РРУ and the gains, for setting up the canceller (not on our model).
- **РЕЖИМЫ and РРУ КШ on 0062-6M1 (high, p50, p98, p160-161):** the noise-figure check (C4).
- **ШИР. — СРЕДН. (high, p104, p111, p118, p140):** the missile tracker's medium beam by hand only in РР (maintenance);
  in combat «+27 В Автозахват» switches it, lamp СР. Л.
- **СВЕТОФИЛЬТР (high, p101, p104):** a motor-driven filter wheel, each press blue, clear, orange; it changes the
  target's contrast; when to use which: not stated.

## C4: function checks and faults

- **The ФК set (high, p157-162):** РОД РАБОТЫ to ФК on 0004-11M1 (in БР the ФК circuits have no power, p158); the
  command transmitters into the dummy load (p141). Checks: noise figure, IF signals in ШТ and СДЦ, clutter compensation,
  the error signals' phase and scale, the computer's static problem, the coders' commands K1-K3, the range trackers, the
  computer's transients, the stabilisation, transmitter powers. At most 10 min (mon_2112_80 p19-20); not to delay fire
  (p251). No healthy values are given anywhere: "within the set limits".
- **0004-12M1 (p158-162):** АНОД ВКЛ.; КОНТРОЛЬ СИСТЕМ chooses the system; КЗ gives command K3 and its lamp lights if the K3
  channel is sound; СМЕНА ЗНАКА and УРОВНИ КОМАНД give K1/K2 levels of either sign, read on ИП1/ИП2 (p159 places ИП1/ИП2
  in the computer: both recorded); ПРОВЕРКА ПЕРЕХОДНЫХ ХАРАКТЕРИСТИК the computer's transients on ИП1/ИП2; ПРОВЕРКА СМЕНЫ
  ЛИТЕРА the commands through a letter change; ПУСК — ИСХ. ПОЛОЖ. "the systems in dynamics" (low).
- **0004-14M (p157, p159, p162):** РЕЖИМЫ: the error signals' phase and scale (СО Ц,Р; СМЕНА ЗНАКА; КОМПЕНС.) and moving
  target plus clutter in MTI (ПП ССЦ, ПП СОЦ); ВЫВОД — НАВЕДЕНИЕ the missile channel's modes; the four meters КАНАЛ Ц,
  КАНАЛ Р judged by the size and steadiness of their readings; ТСЦ zeroes СО β, ε (p100).
- **КОНТР. ИНД. (medium, p66):** the IFF's check marks on the PPI: eight guaranteed-identification marks and one general
  mark in VII–II, one general mark otherwise (the AKM's 1Л24-2). Citation fix: p66, not p53.
- **КОНТРОЛЬ — РАБОТА on ВПУ-55 (high, p102-104):** КОНТРОЛЬ shows the test chart 0249 and puts the crosshair out.
- **УКЧ (medium, p163):** the frequency monitor cuts the load when the supply falls to 360 Hz.
- **The supply (p23, p163, p247):** 220 V ±3.5 %, 400±10 Hz; the voltmeter 218-230 V.
- **0062-6M1 (high, p50, p98, p160-161):** ПОДЖИГ ГШ fires a noise generator; the КОНТРОЛЬ meter reads the channel's
  sensitivity. **0091-17M (p149-150):** КОНТРОЛЬ НАПР. reads +600 V and +18 kV of the command transmitters.
- **Fault lamps (high):** ОТКАЗ СРП dark when the computer is sound (p137, p247); ОТКАЗ Qc lights with a phase of the gyro
  unit's supply lost (p51, p53); ОБРЫВ ФАЗЫ β,γ and ГОН (p50, p96-97); ПРЕД. УГОЛ a stabilisation drive at its limit, search
  stopped and the antenna locked, cleared by stowing on level ground (p48, p50); ПЕРЕГРЕВ ОО04-20М1 the transmitters' or
  OC61-3M1's overheating (p169); the fuse lamps (p166, every system's normal-work list). НЕИСПРАВНОСТЬ: silent.
- **«К бою» (high, p246-247):** the order of the crew's checks before combat; the ФК is not in it.
- **R-123M КОНТРОЛЬ НАПРЯЖЕНИЙ, КАЛИБРОВКА:** nothing in the local sources.

## The checks' model (15 Nov 2026)

What the sources above give, built as `simcore/checks.hpp` and the console's `"checks"` block
(`data/procedures/osa_ak.json`). The sources say what each check shows, never its numbers: every reading below is
SAM3D's (confidence low), chosen so a sound check reads clearly on the scale and a fault reads clearly dead.

- **ИП1 and ИП2 (0004-12M1).** Two centre-zero meters on SŁ-1M (the mesh's two cases, `sl1m_trace` L1). With АНОД ВКЛ.
  in ФК, КОНТРОЛЬ СИСТЕМ picks the check (its legends are unread: SAM3D's reading of the systems p159-162 lists):
  1 the СРП's static task, where ПУСК gives K1 0.6 and K2 -0.4 of the swing ("a certain level", p159) and nothing without
  the computer (ОТКАЗ СРП); 2 the passage of commands, where УРОВНИ КОМАНД or ПРОВЕРКА СМЕНЫ ЛИТЕРА gives both 0.8, negative
  with СМЕНА ЗНАКА, and nothing held gives the zero levels (p159-160); 3 the transients, the channel ПРОВЕРКА ПЕРЕХОДНЫХ
  ХАРАКТЕРИСТИК picks settling at 0.5; 4 and 5 read nothing. A new fault, `command_channel_fault` (K1, K2 do not pass),
  leaves both dead.
- **0004-14M's four meters.** As before by РЕЖИМЫ (they had the same values as levels); now ВЫВОД swings the missile
  pair 1.6 times НАВЕДЕНИЕ (the gathering beam's wider scale), and ЧАСТОТА, the target channel's quartz oscillator (p162),
  drops the target pair linearly to nothing 0.2 of its turn off mid-scale. A new fault, `missile_error_fault`, leaves the
  missile pair dead as `error_channel_fault` does the target pair.
- **The noise check (0062-6M1).** РЕЖИМЫ picks the channel (1 the search radar's ГШ-2, 2 and 3 the tracking radar's two
  ГШ-5 channels, p161 "separately by two channels"; 4 and 5 nothing: КАНАЛЫ, on the large knob, is not a control). Its
  РРУ КШ knob (ОСН. for the first two, ДОП. for the third: SAM3D's pairing) sets the receiver's own noise on КОНТРОЛЬ, 0.6
  of the swing at full knob; ПОДЖИГ ГШ held raises it by the Y-factor, twice for a sound receiver, 1 + 1/F for one whose
  noise figure is F worse (`degraded_db` 6 dB: 1.25). The drill: set the knob so the noise alone sits on a mark, fire the
  generator, read the rise. The receivers' input stages are on with the three-minute relay (p165), so the check waits for
  it. New faults `soc_receiver_degraded`, `ssc_receiver_degraded`.
- **What the faults cost.** A degraded receiver loses `degraded_db` on every echo and on the clutter in the EM layer
  (`em::RadarState::rx_loss_db`); without it, the radar's reach falls to 10^(-6/40) = 0.71 (the radar equation's fourth
  root). A guidance fault set at the meeting multiplies the shot's chance (`data/gameplay/osa_ak.json`
  `outcome.fault_factors`, game values): K3 not passing 0 (the fuse's arming command, p160), K1/K2 not passing 0 (no
  commands), the target's error signals 0.5, the missile's 0.4. The debrief names the fault.
- **РЕЛЕ ТЕПЛ. (0096-9M1, p99, p166).** The thermal relays (two phases, 35 A) guard the ЭМУ's drive motor; the switch at
  ВКЛ. (its start position) keeps them in, ОТКЛ. shunts their block contacts "in case of need". A new fault,
  `emu_overload` (a binding bearing, ice on the turret), heats them: after 20 s (a game value) they open and the turret
  stops in azimuth, ПРИВОД βн's lamp going dark; ПРИВОД βн off and on resets them (SAM3D's reading), and with the overload
  still there they open again after another 20 s. Shunted, the turret turns on through the overload, and after 5 min
  (a game value) the motor burns out until the fault is cleared (the repair, not modelled).
- **R-123M (bsuir_r123m_2019, registered since).** КОНТРОЛЬ НАПРЯЖЕНИЙ has a БС position reading the vehicle's network
  (norm 23-28 V in the meter's shaded sector), and the set is calibrated after 20 min, required at or below 0 °C. Not built:
  the set's meter is not a display in the cabin and the selector's positions (from the plate) have no БС; left with C7.
