"""Rows and constants owned by the dotted-expression fixture mode."""

# Dotted-expression probe.  Each row is (key, expression, contexts): "C" runs
# the expression in the player's login trigger (default object and SRC are the
# character), "I" in the @Equip trigger of an equipped item (default object is
# the item, SRC is the character).  tools/fixtures/test_dotted_expressions.py
# holds the expected values.
DOTTED_PROBE_ACCOUNT = "DottedProbe"
DOTTED_PROBE_ITEM_ID = 0x0E7B
DOTTED_PROBE_DISPOSABLE_ID = 0x0E7C
DOTTED_PROBE_FINDID_ID = 0x0E7D
DOTTED_PROBE_LAYER = 30
DOTTED_PROBE_SECTOR_LIGHT = 4
# Loops in the probe that run into the WHILE/FOR iteration limit.
DOTTED_PROBE_CAPPED_WHILE = "WHILE (2>1)"
DOTTED_PROBE_CAPPED_FOR = "FOR 20000"
DOTTED_PROBE_MARKER = "SPHERE_DOTTED_EXPR"

DOTTED_EXPRESSION_ROWS = (
    # Forms without a function-root chain; their results must not change.
    ("src_name", "<src.name>", "CI"),
    ("src_str", "<src.str>", "C"),
    ("src_serial", "<src.serial>", "CI"),
    ("serv_name", "<serv.name>", "C"),
    ("var_paren", "<var(dotted_probe_var)>", "C"),
    ("eval_decimal", "<eval 5*1.5>", "CI"),
    ("eval_decimal_zero", "<eval 30.0>", "C"),
    ("eval_paren_decimal", "<eval(2.5)>", "C"),
    ("eval_nested", "<eval <src.str>*1.5>", "C"),
    # HVAL keeps 0.99's 32-bit, leading-zero lower-case hexadecimal format
    # in both space-separated and parenthesized function forms.
    ("hval_space", "<hval 0xBEEF>", "CI"),
    ("hval_paren", "<hval(-1)>", "C"),
    # Sphere's character-class syntax is used by the stock input guards.  A
    # class consumes exactly one character, so the two-digit form accepts
    # digits and the malformed/incorrect-length forms stay rejected.
    ("strmatch_class_digits", "<STRMATCH 50,[-0123456789][-0123456789]>", "C"),
    ("strmatch_class_letters", "<STRMATCH qwd,[-0123456789][-0123456789]>", "C"),
    ("strmatch_class_mixed", "<STRMATCH 5a,[-0123456789][-0123456789]>", "C"),
    ("strmatch_class_short", "<STRMATCH 5,[-0123456789][-0123456789]>", "C"),
    ("strmatch_class_long", "<STRMATCH 500,[-0123456789][-0123456789]>", "C"),
    ("strmatch_class_negative", "<STRMATCH -50,[-0123456789][-0123456789][-0123456789]>", "C"),
    # This is the shard's exact fixNumber body, including the dynamic pattern
    # assembled by ARG in its WHILE loop.  Keep the function call in the
    # fixture so the real nested escape path is covered, rather than testing
    # STRMATCH in isolation.
    ("fixnumber_exact_valid", "<fixNumber(50)>", "C"),
    ("fixnumber_exact_valid_100", "<fixNumber(100)>", "C"),
    ("fixnumber_exact_valid_75", "<fixNumber(75)>", "C"),
    ("fixnumber_exact_valid_25", "<fixNumber(25)>", "C"),
    ("fixnumber_exact_qwd", "<fixNumber(qwd)>", "C"),
    ("fixnumber_exact_mixed", "<fixNumber(5a)>", "C"),
    ("fixnumber_exact_negative", "<fixNumber(-50)>", "C"),
    ("fixnumber_exact_empty", "<fixNumber()>", "C"),
    ("fixnumber_exact_positive_negative", "<fixNumberPositive(-50)>", "C"),
    # FINDRES returns a typed resource reference so its properties can be
    # read through the same dotted-expression path as world objects.
    ("findres_spell_mana", "<findres(spell,s_fixture_heal).manause>", "C"),
    ("findres_spell_runes", "<findres(spell,s_fixture_heal).runes>", "C"),
    # Nested results are deliberately longer than their source tags.  The
    # trailing text must survive the outer replacement in both trigger
    # contexts.
    ("nested_escape_suffix", "prefix <STRMATCH <NAME>,*> suffix", "CI"),
    ("nested_escape_after_nested", "left <STRLEN <NAME>> right", "CI"),
    ("strlen_dot", "<strlen a.b>", "CI"),
    ("strcmp_dot", "<strcmp a.b,a.b>", "C"),
    ("strindexof_dot", "<strindexof abc.def,def>", "C"),
    # 0.99 string helpers use zero-based offsets and token indexes.
    ("strmid_dot", "<strmid abcdef,2,3>", "CI"),
    ("strgettok_dot", "<strgettok alpha|beta|gamma,1,|>", "C"),
    ("strmid_quoted", '<strmid "abcdef",2,3>', "C"),
    ("strgettok_quoted", '<strgettok "alpha,beta,gamma",1,",">', "C"),
    ("safe_src_name", "<safe src.name>", "C"),
    ("safe_missing_tag", "<safe src.tag(probe_missing)>", "C"),
    ("safe_missing_tag_direct", "<safe.tag(probe_missing)>", "C"),
    ("tag_paren", "<tag(probe_text)>", "CI"),
    ("function_plain", "<f_dotted_serial>", "CI"),
    ("serial", "<serial>", "I"),
    # In item callbacks UID is the current item object, so its dotted
    # properties and methods must run against that object rather than the
    # scalar serial value returned by the legacy UID property.
    ("uid_name", "<uid.name>", "I"),
    ("deferred_src_tag", "<?src.tag(probe_text)?>", "C"),
    ("deferred_eval", "<?eval 5*1.5?>", "C"),
    ("deferred_strlen", "<?strlen a.b?>", "C"),
    # One-level references whose last segment carries its own arguments or
    # is a script function evaluated with the reference as default object.
    ("src_tag_paren", "<src.tag(probe_text)>", "CI"),
    ("src_tag_paren_num", "<src.tag(probe_num)>", "C"),
    ("src_tag_paren_missing", "<src.tag(probe_missing)>", "C"),
    ("src_function", "<src.f_dotted_serial>", "CI"),
    ("src_function_args", "<src.f_dotted_arg(5)>", "C"),
    ("function_root_tag", "<f_dotted_serial.tag(probe_text)>", "CI"),
    ("finduid_missing_name", "<finduid(0bad0bad).name>", "C"),
    # Function roots with arguments and multi-level chains.
    ("src_account_name", "<src.account.name>", "CI"),
    ("findaccount_name", "<findaccount(" + DOTTED_PROBE_ACCOUNT + ").name>", "C"),
    ("finduid_name", "<finduid(<src.serial>).name>", "C"),
    ("finduid_serial", "<finduid(<src.serial>).serial>", "C"),
    ("finduid_tag", "<finduid(<src.serial>).tag(probe_text)>", "C"),
    ("finduid_function", "<finduid(<src.serial>).f_dotted_serial>", "C"),
    ("uid_alias_name", "<uid(<src.serial>).name>", "C"),
    ("findid_bare_item", "<src.findlayer(layer_pack).findid(i_dotted_findid).serial>", "C"),
    ("lastnewitem_name", "<serv.lastnewitem.name>", "C"),
    ("function_args_root", "<f_dotted_arg(<src.serial>).name>", "C"),
    ("function_args_chain", "<f_dotted_arg(<src.serial>).findlayer(30).serial>", "C"),
    ("src_findlayer_name", "<src.findlayer(30).name>", "CI"),
    ("src_findlayer_serial", "<src.findlayer(30).serial>", "CI"),
    ("src_findlayer_tag", "<src.findlayer(30).tag(probe_text)>", "C"),
    # Stock 0.99 also accepts a dotted argument segment: FINDLAYER.21 is
    # equivalent to FINDLAYER(21).  Keep the layer argument and the following
    # property separate so this legacy form cannot be reported as an unknown
    # FINDLAYER.* chain.
    ("src_findlayer_pack_serial", "<src.findlayer(layer_pack).serial>", "C"),
    ("src_findlayer_legacy_serial", "<src.findlayer.layer_pack.serial>", "C"),
    ("src_findlayer_legacy_numeric_serial", "<src.findlayer.30.serial>", "C"),
    ("findid_root_serial", "<findid(i_dotted_findid).serial>", "C"),
    ("src_sector_light", "<src.sector.light>", "C"),
    ("deferred_finduid_name", "<?finduid(<src.serial>).name?>", "C"),
    ("deferred_findlayer_serial", "<?src.findlayer(30).serial?>", "C"),
    # Chains rooted at a reference property of the default object, and
    # dotted TAG.name / TAG0.name reads.
    ("sector_light", "<sector.light>", "C"),
    ("cont_name", "<cont.name>", "I"),
    ("cont_tag", "<cont.tag(probe_text)>", "I"),
    ("topobj_name", "<topobj.name>", "I"),
    ("tag_dot", "<tag.probe_text>", "CI"),
    ("src_tag_dot", "<src.tag.probe_text>", "CI"),
    ("src_tag_dot_missing", "<src.tag.probe_missing>", "C"),
    ("src_tag0_dot_missing", "<src.tag0.probe_missing>", "C"),
    ("src_tag0_dot_num", "<src.tag0.probe_num>", "C"),
    # Bare reference operands in numeric expressions.
    ("eval_bare", "<eval src.str+1>", "C"),
    ("eval_bare_mixed", "<eval <src.str>+src.dex>", "C"),
    ("eval_bare_negative", "<eval -src.str>", "C"),
    ("eval_bare_tag", "<eval src.tag.probe_num*2>", "CI"),
    ("eval_bare_function", "<eval src.f_dotted_serial>", "I"),
    ("eval_bracket_str_dex", "<eval <src.str>+<src.dex>>", "C"),
    ("eval_defname", "<eval dotted_probe_const>", "C"),
    ("eval_unknown_reference", "<eval foo.bar>", "C"),
    # Expression grammar: parentheses, unary !, && and ||.  Arithmetic and
    # comparison operators chain from right to left without precedence.
    ("eval_paren_group", "<eval (1+2)*3>", "C"),
    ("eval_paren_right", "<eval 2*(3+4)>", "C"),
    ("eval_not", "<eval !0>", "C"),
    ("eval_and", "<eval 1 && 1>", "C"),
    ("eval_or_false", "<eval 0 || 0>", "C"),
    ("eval_chain_left", "<eval 10-3-2>", "C"),
    ("eval_chain_no_precedence", "<eval 1+2*3>", "C"),
    ("eval_chain_compare", "<eval 3==1+2>", "C"),
    # Unresolved on every build so far (VAR.name reads are not implemented).
    # Not asserted; it checks that the unknown-keyword report keeps the
    # normalized legacy key.
    ("unresolved_var_dot", "<var.dotted_probe_var>", "C"),
)

DOTTED_CONDITION_ROWS = (
    ("cond_src_str_eq", "(src.str==<src.str>)"),
    ("cond_src_str_gt", "(src.str>10)"),
    ("cond_src_str_lt", "(src.str<10)"),
    ("cond_chain_arith", "(src.str+5>src.dex)"),
    ("cond_sector_light", "(sector.light==4)"),
    ("cond_tag_set", "(src.tag.probe_num==7)"),
    ("cond_tag_paren", "(src.tag(probe_num)==7)"),
    ("cond_tag_unset", "(src.tag.probe_missing==1)"),
    ("cond_tag0_unset", "(src.tag0.probe_missing==0)"),
    ("cond_safe_tag_missing", "(safe.tag(probe_missing))"),
    ("cond_base_tag", "(tag.probe_num==7)"),
    # Object predicates are valid bare operands in script conditions.  Keep
    # this form explicit so the resolver cannot regress to DEFNAME-only
    # lookup when a predicate has no dotted suffix or call parentheses.
    ("cond_isplayer", "(isplayer)"),
    # Bare literals and resource constants must retain their legacy numeric
    # values when the object-reference resolver is considered first.
    ("cond_literal_one", "(1)"),
    ("cond_literal_zero", "(0)"),
    ("cond_literal_hex", "(0a)"),
    ("cond_defname_bare", "(dotted_probe_const)"),
    ("cond_item_id", "(i_dotted_probe)"),
    ("cond_type_id", "(t_eq_script)"),
    # STR is both a real property and a fixture DEFNAME.  The historical
    # DEFNAME lookup wins, as it did before the bare-reference change.
    ("cond_property_defname", "(str)"),
    # A bare declared function stays on the legacy numeric path: its body is
    # not executed while reading a condition.  An unknown name remains zero.
    ("cond_bare_function", "(f_dotted_bare_probe)"),
    ("cond_unknown_bare", "(dotted_missing_name)"),
    ("cond_findlayer", "(src.findlayer(30))"),
    ("cond_findlayer_empty", "(src.findlayer(9))"),
    ("cond_finduid_name", "(finduid(<src.serial>).name==<src.name>)"),
    ("cond_bare_name_other", "(src.name==Other)"),
    ("cond_bracket_name_other", "(<src.name>==Other)"),
    ("cond_defname", "(dotted_probe_const==1234)"),
    ("cond_unknown_reference", "(foo.bar)"),
    ("cond_bare_and", "(src.str>10) && (src.dex>10)"),
    ("cond_bare_paren", "((src.str+5)>src.dex)"),
    ("cond_bare_not", "(!src.tag.probe_missing)"),
    # Expression grammar in conditions.
    ("grammar_and_true", "(1>0) && (2>1)"),
    ("grammar_and_false", "(1>0) && (2<1)"),
    ("grammar_or_true", "(0) || (1)"),
    ("grammar_or_false", "(0) || (0)"),
    ("grammar_not_zero", "(!0)"),
    ("grammar_not_one", "(!1)"),
    ("grammar_not_paren", "(!(1>2))"),
    ("grammar_paren_arith", "(((2+3)*4)==20)"),
    ("grammar_and_or", "(0) && (1) || (1)"),
    ("grammar_or_and", "(1) || (0) && (0)"),
    ("grammar_ge", "(5 >= 3)"),
    ("grammar_ge_equal", "(3 >= 3)"),
    ("grammar_ge_false", "(2 >= 3)"),
    ("grammar_le_equal", "(3 <= 3)"),
    ("grammar_le_false", "(4 <= 3)"),
    ("grammar_unparenthesized", "(1 < 2 && 3 > 2)"),
    ("grammar_escape_terms", "(<src.str> > 10) && (<src.tag(probe_num)> == 7)"),
    ("grammar_bare_terms", "(src.str>10) && (src.tag.probe_num==7)"),
    ("grammar_bare_terms_false", "(src.str>10) && (src.tag.probe_num==8)"),
    ("grammar_not_bare_set", "(!src.tag.probe_num)"),
)
