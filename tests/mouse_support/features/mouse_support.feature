Feature: Mouse support in interactive terminal apps
  The desktop automation harness should be able to drive retro-term with
  tmux and mc in mouse mode and observe the expected side effects.

  Scenario: tmux mouse mode changes the active pane on click
    Given the retro term app is running the "tmux_mouse_fixture.sh" fixture
    When I click inside the tmux pane named "bottom"
    And I press the tmux active-pane capture hotkey
    Then the tmux active pane artifact should equal the "bottom" pane id

  Scenario: mc command buttons react to mouse clicks
    Given the retro term app is running the "mc_mkdir_fixture.sh" fixture
    When I click the mc command button "mkdir"
    And I type "mouse-created-dir" and press enter
    And I press the key "f10"
    Then the directory "mouse-created-dir" should exist inside the mc fixture root
