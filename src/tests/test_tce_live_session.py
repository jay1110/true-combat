import sys, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from tce_live_session import player_deployed, atomic_text, slot_script

class LiveSessionTests(unittest.TestCase):
 def test_assigned_weapon_does_not_mean_deployed(self):
  s=dict(snapshot_weapon=45,weapon_time=0,pm_type=0,pm_flags=0)
  self.assertTrue(player_deployed(s,45))
  for field,value in [('snapshot_weapon',9),('weapon_time',100),('pm_type',4),('pm_flags',4096),('pm_flags',16384)]:
   changed={**s,field:value};self.assertFalse(player_deployed(changed,45),(field,value))
 def test_failed_publish_keeps_engine_mailbox_intact(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'mailbox.cfg';p.write_text('previous')
   with patch('tce_live_session.os.replace',side_effect=OSError('busy')):
    with self.assertRaises(OSError):atomic_text(p,'next')
   self.assertEqual(p.read_text(),'previous')
   atomic_text(p,'next');self.assertEqual(p.read_text(),'next')
 def test_slot_exhaustion_cannot_replay_an_old_action(self):
  for i in [-1,64]:
   with self.assertRaises(ValueError):slot_script(i,'kill')
  self.assertTrue(slot_script(0,'kill').startswith('set tce_slot_0 ""\n'))
  self.assertIn('writeconfig live_done_0.cfg',slot_script(0,'kill'))
if __name__=='__main__':unittest.main()
