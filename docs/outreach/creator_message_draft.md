# Draft: message to the SAM Simulator creator ("Hpasp")

**Status: draft for Nicholas to review and send himself.** Nothing has been sent. Suggested channel: the contact on
his site, or a private message on the SimHQ SAM Simulator forum.

---

Subject: SAM3D — a 3D Osa game built on SAM Simulator, and a few questions

Dear Hpasp,

I'm Nicholas, a hobby developer in Australia, and a long-time admirer of SAM Simulator. As you know, I'm building
SAM3D with your blessing: a first-person 3D game in Unreal Engine that takes SAM Simulator's "realistic to the
switch" consoles into a 3D cabin. The first system is your 9K33M2 Osa-AK, played as a battery: the player can drive
the vehicle, deploy it, sit in any of the five crew seats or the PU-12, and NPC crew work the same controls. It's
non-commercial so far, and I credit SAM Simulator as its foundation.

So far the Osa-AK consoles, procedures and instruments work in 3D (search scope, range indicator, TV, dials), built
from your manual and your 2013 SimHQ development thread, cross-checked with public NVA, Soviet and Polish material.
A few questions, if you have time:

1. **Source or notes.** Would you share the VB6 source of the Osa, or notes on how its consoles, lamps and guidance
   phases behave? I'd use them only for SAM3D and credit you.
2. **Art and sounds.** May I use or adapt your Osa panel art and sounds in SAM3D? If you prefer not, I'll make
   original art.
3. **Details where sources disagree.** I'd value your view on:
   - ЗАХВАТ and ВКЛ ВЫС: your manual has them light during missile preparation; your forum answer and an NVA account
     place ЗАХВАТ after launch (missile captured in the wide beam) and ВКЛ ВЫС at ВНИМАНИЕ.
   - The middle search scale: 0–35 km (your manual) or 0–30 km (an NVA source)?
   - Missile preparation: 15 s (manual), 13 s (NVA) or 12 s?
   - The TV lens fields: 1.5° / 5° (manual) or 1 × 1.4° / 3.5 × 4.7° (an operator on your forum)?
4. **Controls I can't identify.** From your manual's panel photos and a photo catalogue of the NVA's vehicles:
   - On the launch console: what do СМЕНА РКТ (under the latch), КАНАЛЫ (КАН. I / КАН. II / СОВМ.) and the
     СР.Л. / ШИР. toggle do, and are РР and ФК the other РОД РАБОТЫ positions (ФК for the function check)?
   - On the elevation block: what does the АЗ button do, next to ЦУ and РУ? In your game ЦУ lights both ЦУ and АЗ,
     and pressing АЗ on its own does nothing.
   - On the range indicator: what does the ЦЕЛЬ selector do, and on the range-tracking block the two ПЕЛЕНГ buttons?
   - The red dots: do they mark each switch's working position for combat, rather than where it starts? (In your
     game РОД РАБОТЫ and РАБОТА / КОНТРОЛЬ start away from their dots, and КАНАЛЫ starts on its dot at СОВМ.)
   - What does the elevation indicator (by the TV monitor) show? In your game its trace sits low until the tracking
     transmitter comes on, then centred, with the target as a bright mark: is the vertical position the elevation
     error, and when does the operator use it?
   - The switch-on panels to the left of the range operator, which you left out on purpose: do you have their
     labels and the switch-on order, and which crew member works them?
   - Where is the launcher's (АПУ) travel lock, and who works it? What does the commander have in front of him
     besides the radio?
5. **Operators.** Would any of the Osa operators you worked with be willing to look at SAM3D's Osa and tell me what's
   wrong?

Thank you for SAM Simulator, and for any help you can give.

Best regards,
Nicholas
