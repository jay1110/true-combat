"""Combat summaries must retain native failures without a script failure event."""
import sys
from pathlib import Path
from unittest import TestCase, main

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tce_nav_livecheck import classify


class LiveDiagnosticsTests(TestCase):
    def test_native_only_failures_remain_failures(self):
        ordinary = '^2TCE2PATH BLOCKED_ORDINARY bot=9 current=80 next=0 reason=centre_timeout'
        destination = '^2TCE2PATH BLOCKED_DESTINATION bot=9 reason=no_supported_target'
        door = '^2TCE2DOOR BLOCKED bot=1 entity=378'
        script = 'FAILURE_DETAIL bot=2 goal=EXPLORE'
        normal = ['TCE2PATH CENTRE_APPROACH bot=9', 'TCE2PATH CENTRE_CLEAR bot=9',
                  'TCE2PATH DESTINATION_VOLUME bot=9', 'TCE2DOOR USE bot=1']
        result = classify('\n'.join(normal + [ordinary, destination, door, script]), '')
        self.assertEqual(result['failures'], [ordinary, destination, door, script])
        self.assertEqual(result['errors'], [])

    def test_literal_gamemonkey_vm_and_compile_diagnostics_are_errors(self):
        messages=['attempt to call non function type','stack overflow','stack underflow',
                  'getdot failed.',"getdot failed on 'null.GetAxisLength'",'setdot failed.',
                  "setdot failed on 'table.value'",'setind failed.','getthis failed. this is null',
                  'setthis failed.','foreach expression has no iterator function',
                  'foreach expression is not table type','unary operator ! for type table error',
                  'unary operator ~ undefined for type table','operator + bad operands int and string',
                  'operator [] bad operand table for null','operator + undefined for type int and string',
                  'operator + failed','User break. Execution halted.','error (142) syntax error',
                  'Could not parse file scripts/goals/goal_checkstuck.gm']
        for message in messages:
            line='^1'+message
            with self.subTest(message=message):self.assertEqual(classify(line,'')['errors'],[line])

    def test_actual_checkstuck_stack_counts_only_vm_error_not_source_or_callstack(self):
        error='^1attempt to call non function type'
        source='scripts/goals/goal_checkstuck.gm(142) : widthX = (ownBox.GetAxisLength("x") + otherBox.GetAxisLength("x"))*0.5-2;'
        lines=[error,'^1',source,'^1','callstack..','^1scripts/goals/goal_checkstuck.gm(142): TrafficYield',
               '^1scripts/goals/goal_checkstuck.gm(260): GetPriority','Rcon from 127.0.0.1:62814:','navprobe']
        result=classify('\n'.join(lines),'')
        self.assertEqual(result['errors'],[error]);self.assertEqual(result['failures'],[])

    def test_vm_patterns_do_not_mistake_source_chat_commands_or_ordinary_navigation_for_errors(self):
        lines=['Bob: attempt to call non function type','bot script_run print("attempt to call non function type");',
               'scripts/goals/check.gm(12) : print("stack overflow");','stack overflow check succeeded',
               'TCE2TRAFFIC YIELD bot=1','TCE2PATH CENTRE_CLEAR bot=1','NAVPATH SUCCESS 1',
               'TCE2PATH BLOCKED_ORDINARY bot=9 reason=centre_unreachable',
               'TCE2TACTIC BEGIN bot 2 goal DEFEND_NavStudio_T2_4495 seconds null','error (0)']
        self.assertEqual(classify('\n'.join(lines),'')['errors'],[])

    def test_vm_literals_are_bound_to_primary_vendor_messages(self):
        root=Path(__file__).resolve().parents[3]
        vm=(root/'src/vendor/omni-bot/source/Omnibot/dependencies/gmscriptex/gmsrc_ex/src/gm/gmThread.cpp').read_text()
        for literal in ('attempt to call non function type','stack overflow','stack underflow',
                        'getdot failed.','setind failed.','getthis failed. this is null',
                        'foreach expression has no iterator function','operator %s bad operands %s and %s'):
            with self.subTest(literal=literal):self.assertIn('"'+literal+'"',vm)


if __name__ == '__main__':
    main()
