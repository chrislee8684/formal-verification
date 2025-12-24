# Liveness Property 1
prism good_prism_model.pm liveness.ltl -prop 1 `
  -exportadv prism_results\good_model\liveness_ce_prop1.tra `
  -exportstates prism_results\good_model\liveness_ce_prop1.sta

# Liveness Property 2
prism good_prism_model.pm liveness.ltl -prop 2 `
  -exportadv prism_results\good_model\liveness_ce_prop2.tra `
  -exportstates prism_results\good_model\liveness_ce_prop2.sta

# Liveness Property 3
prism good_prism_model.pm liveness.ltl -prop 3 `
  -exportadv prism_results\good_model\liveness_ce_prop3.tra `
  -exportstates prism_results\good_model\liveness_ce_prop3.sta

# Liveness Property 4
prism good_prism_model.pm liveness.ltl -prop 4 `
  -exportadv prism_results\good_model\liveness_ce_prop4.tra `
  -exportstates prism_results\good_model\liveness_ce_prop4.sta

  # Liveness Property 1
prism good_prism_model.pm liveness.ltl -prop 1 `
  -exportadv prism_results\good_model\liveness_ce_prop1.tra `
  -exportstates prism_results\good_model\liveness_ce_prop1.sta

  # Safety Property 1
prism good_prism_model.pm safety.pctl -prop 1 `
  -exportadv prism_results\good_model\safety_ce_prop1.tra `
  -exportstates prism_results\good_model\safety_ce_prop1.sta

# Safety Property 2
prism good_prism_model.pm safety.pctl -prop 2 `
  -exportadv prism_results\good_model\safety_ce_prop2.tra `
  -exportstates prism_results\good_model\safety_ce_prop2.sta