# The PU-12M's "SAU block": what it might have been (October 2026)

One sentence on the NVA veterans' site says that before the PU-12Ms passed to the Bundeswehr in 1990 "the SAU blocks
(block for cooperation with the frontal aviation) were removed and returned to the Soviet Union as sensitive
technology" (`flak11`, pages PU12-0 and 9S482; copied word for word by `panzerregiment4`). This note follows that
sentence through German, Russian and Czech sources. Read 22 Oct 2026.

**Short answer.** Every document that names a САУ / SAU in the PU-12M means **the R-111's antenna matching unit**
(согласующее антенное устройство), including a drawing on the same NVA site. No document of any operator names a
block for cooperation with aviation in the PU-12(M). What the GDR did hand back in September 1990 as "sensitive
technology" was **special communications (crypto) equipment, IFF equipment and coding documents**. The likeliest
reading is that something of that kind was taken out of the PU-12Ms and was later remembered under the wrong label.
It cannot be settled from public sources. SAM3D still leaves it out.

## 1. What САУ is in every document that draws or lists one

| Source | What it says |
|---|---|
| `kshm_tgtu` (Russian signals textbook on command-staff vehicles, 2004), list of abbreviations | «САУ – согласующее антенное устройство к радиостанции Р-111». In the R-111 KShMs it is a box of its own, the «блок согласующего антенного устройства с антенным распределителем», with a separate «блок автоматики САУ». In the twin version two R-111s share one whip through it |
| `r111_to` (the R-111's ТО) | The front's САУ button and НАСТРОЙКА САУ lamp, the ИНДИКАТОР's НАСТР. САУ position, the АВТ. САУ connector, the antenna distributor ШТЫРЬ / ТЕЛЕСКОП (`docs/research/PU12_RADIOS_2026-10.md`) |
| `flak11` PU12-1: the signals drawing from the *Handbuch für Fla-Einheiten* (1990) | Item 13 *Automatikblock der Antennenanpassung*, item 14 ***Antennenanpasseinrichtung SAU***, item 15 the whip |
| `csla_pu12_manual`, the plan of the equipment | Item 26, SAU, in the battery commander's corner beside the R-407 |

So the same author, on the next page, labels SAU as the antenna matching unit. His PU12-0 sentence is the only place
anywhere that gives SAU another meaning, and it names no source for that sentence. His literature list has handbooks
and studies, not the 1990 handover papers.

## 2. No operator lists a block for cooperation with aviation

- **The ČSLA list of every item** (`valka_pu12`, transcribing the ČSLA's *PVOV-2-3 Bojová služba u PU-12(M)* and
  TMNO 24-65-86): the ASPD-12 or ASPD-U blocks (R, S, P, SSP, BP, IT, UO, DI, DTP; for the PU-12M the RPS-M, 220N, FR,
  IT-45, DT-2, DCh, S23-1), the navigation set, three R-123M, the R-111, the R-407 with its BP-3M, the intercom, the boards,
  power, NBC kit. Nothing for aviation, IFF or crypto. The one odd name, "přístroj 220N", is the ССП synchro drive
  ("ССП or 220Н", `ppt_pu12_lesson`).
- **The NVA lists** (`flak11` PU12-0 and 9S482): the ASPD-U blocks, five radios, navigation, power. Nothing for
  aviation.
- **The Russian training text** (`ru_pu12_text`, the later BTR-80 build) has a *task* that is cooperation with
  aviation: «предупреждение о пролетах своей авиации в зонах боевых действий подразделений ПВО» (warning the air defence
  units of their own aviation's flights through their zones). The equipment that does it is the ASPD-U: each target
  carries the «свой / чужой» characteristic, and commands go out as two-digit codes. That is a function of the
  automation, not a separate block.

## 3. What the GDR actually handed back in 1990

- **The NVA's own proposal** (`nva_sensitive_1990`, an excerpt photographed on `flak11` 9K33-5): *Vorschlag zur
  weiteren Verwendung bzw. Vernichtung sensitiver Dokumente, Technik und Bewaffnung der NVA*, with columns to destroy,
  return free to the USSR or sell to the Western Group, by 10-15 Sept 1990. The visible part lists the PASUV field
  command system with its coding means; for the Mi-24, "Geräte und Unterlagen des Kennungssystems PAROL (KHG-5)",
  returned free; the 9K79 Tochka; the 9K33 Osa-AK and 9K35 Strela-10. The page that would list the PU-12M is not shown.
- **The first category of "sensitive technology" was "only special communications equipment and documentation"**
  (`kuestenraketen_krr18`, a regiment commander's account). It was collected and handed to the Soviet forces in three
  weeks at the end of September 1990. His regiment handed over its voice-encryption sets.
- **IFF** (`dewiki_p15`): the P-15 radars' already-fitted IFF components were "returned to the Soviet Union in 1990 as
  so-called sensitive technology". That is the same wording as Wagner's sentence.

## 4. The candidates, most likely first

1. **Special communications (crypto) equipment, remembered under the tuner's label.** Both of the post's long-haul sets
   had inputs for it. The R-111 has its ТЛК input on the line (`r111_to`), and the R-407 has "the channel's input for
   special equipment" (`r407_to`). In Soviet signals usage «СА» is «специальная аппаратура», the secure equipment
   (`kshm_tgtu`'s abbreviations). Crypto was the core of the 1990 "sensitive" category, and an R-111 command net to the
   regiment is where a secure unit would sit. Against it: the "cooperation with aviation" gloss does not fit crypto.
2. **IFF-related equipment.** IFF is the Soviet means of working with friendly aviation, it went back in 1990, and
   Wagner's wording matches the IFF sentence. Against it: the PU-12M has no interrogator of its own. Identity reaches it
   from a cabled radar's *Kennung* line or as the telecode's characteristic (`flak11` figure 2;
   `docs/research/PU12_RESEARCH_2026-10.md`). The IFF hardware and its codes sit in the radars (the command battery's
   P-18, P-19, P-40), not in the post. A PU-12M of the command battery could have carried something for the Parol
   change-over planned for 1989-90, but nothing says so.
3. **The R-111's tuner itself, taken literally.** The R-111 and its tuner were ordinary signals equipment, in every
   R-111 command vehicle, and NVA R-111 sets are in collectors' hands today (`r111_refs`). Nothing makes the tuner
   "sensitive". Unlikely.
4. **A function, not a block.** The writer may have remembered the post's task of warning about friendly flights (§2)
   and tied it to the removed boxes. This fits with 1 or 2.

## 5. How it could be settled

- The handover papers: the MfAV's *Plan der Maßnahmen zur Verwendung bzw. Vernichtung sensitiver Dokumente, Technik und
  Bewaffnung der NVA* (1 Sept 1990) and its annex of 14 Sept 1990, in the Bundesarchiv (Freiburg, NVA holdings). The
  page for the troop air defence's command equipment would name what came out of the PU-12M.
- The nva-forum user "mathias", who supplied the excerpt to `flak11`, may have the whole document.
- R. Wagner (`flak11`'s webmaster) could say where his sentence came from.
- The 1990 *Handbuch für Fla-Einheiten* (`nva_handbooks`, to acquire) has the full signals list. The DNB holds it in
  Leipzig (1990 A 24042), and used copies sell for about EUR 30 (ZVAB, seen 22 Oct 2026). The DDR Museum's five
  photographed spreads do not include the PU-12.

## What this changes in SAM3D

Nothing in the model. The R-111's САУ stays as built (`data/procedures/pu12m.json` `radio.fug3.sau`, the antenna
distributor on its block at the battery commander's place). No aviation-cooperation block is added, since no operator
document describes one. If secure voice on the regiment's net is ever wanted, it is candidate 1 above. It would need
its own sources first.
