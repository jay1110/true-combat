from unittest import TestCase,main
from mission_catalog import script_blocks,callback


class MissionCatalogTests(TestCase):
    def test_callback_must_belong_to_real_entity_block_not_comment_or_other_goal(self):
        script='''// fake { dynamited { } }
game_manager { spawn { wm_announce "ignored { braces }" } }
crate_A { spawn { constructible_class 3 } dynamited { trigger game_manager bomb_planted }
 defused { trigger game_manager bomb_defused } death { trigger game_manager bomb_detonated } }
crate_B { spawn { constructible_class 3 } }
'''
        blocks=script_blocks(script)
        self.assertEqual(set(blocks),{'game_manager','crate_A','crate_B'})
        self.assertEqual(callback(blocks['crate_A'],'dynamited'),1)
        self.assertEqual(callback(blocks['crate_B'],'dynamited'),0)
        self.assertEqual(callback('wm_announce "dynamited { } http://test"','dynamited'),0)


if __name__=='__main__':main()
