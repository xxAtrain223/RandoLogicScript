const PREC = {
  TERNARY: 1,
  OR: 2,
  AND: 3,
  COMPARISON: 4,
  ADD: 5,
  MULTIPLY: 6,
  UNARY: 7,
  CALL: 8,
  MEMBER: 9
};

module.exports = grammar({
  name: "rls",

  extras: ($) => [/[\s\r\n]/, $.comment],

  word: ($) => $.identifier,

  rules: {
    source_file: ($) => repeat($.declaration),

    comment: () => token(seq("#", /.*/)),
    identifier: () => /[A-Za-z_][A-Za-z0-9_]*/,
    number: () => /-?\d+/,
    string: () => token(seq('"', repeat(choice(/[^"\\\n]/, /\\["\\]/)), '"')),

    declaration: ($) => choice(
      $.region_declaration,
      $.extend_region_declaration,
      $.define_declaration,
      $.extern_define_declaration,
      $.enum_declaration,
      $.extern_enum_declaration
    ),

    region_declaration: ($) => seq("region", field("name", $.identifier), "{", repeat(choice($.region_data_entry, $.section)), "}"),
    extend_region_declaration: ($) => seq("extend", "region", field("name", $.identifier), "{", repeat($.section), "}"),
    region_data_entry: ($) => seq(field("key", $.identifier), ":", field("value", $.expression)),

    section: ($) => seq(field("kind", choice("events", "locations", "exits")), "{", repeat($.entry), "}"),
    entry: ($) => seq(field("name", $.identifier), ":", field("value", $.expression)),

    define_declaration: ($) => seq("define", field("name", $.identifier), "(", optional($.parameters), ")", ":", field("body", $.expression)),
    extern_define_declaration: ($) => seq("extern", "define", field("name", $.identifier), "(", optional($.parameters), ")", "->", field("return_type", $.identifier)),
    parameters: ($) => commaSep1($.parameter),
    parameter: ($) => seq(
      field("name", $.identifier),
      optional(seq(":", field("type", $.identifier))),
      optional(seq("=", field("default", $.expression)))
    ),

    enum_declaration: ($) => seq("enum", field("name", $.identifier), "{", optional(commaSep1($.enum_member)), "}"),
    extern_enum_declaration: ($) => seq("extern", "enum", field("name", $.identifier), "{", optional(commaSep1($.extern_enum_entry)), "}"),
    enum_member: ($) => seq(field("name", $.identifier), optional(seq("=", field("value", $.number)))),
    extern_enum_entry: ($) => choice($.enum_member, $.glob_pattern),
    glob_pattern: () => token(/[A-Za-z0-9_]*\*[A-Za-z0-9_*]*/),

    expression: ($) => choice($.ternary_expression, $.binary_expression, $.unary_expression, $.primary_expression),
    primary_expression: ($) => choice(
      $.boolean,
      "here",
      $.string,
      $.number,
      $.call_expression,
      $.member_expression,
      $.match_expression,
      $.list_expression,
      $.identifier,
      seq("(", $.expression, ")")
    ),
    boolean: () => choice("true", "false", "always", "never"),
    list_expression: ($) => seq("[", optional(commaSep1($.expression)), "]"),
    call_expression: ($) => prec(PREC.CALL, seq(field("function", $.identifier), "(", optional(commaSep1($.argument)), ")")),
    argument: ($) => choice($.named_argument, $.expression),
    named_argument: ($) => seq(field("name", $.identifier), ":", field("value", $.expression)),
    member_expression: ($) => prec(PREC.MEMBER, seq(field("object", $.identifier), ".", field("member", $.identifier))),
    unary_expression: ($) => prec(PREC.UNARY, seq("not", field("argument", $.expression))),
    binary_expression: ($) => choice(
      prec.left(PREC.MULTIPLY, seq($.expression, choice("*", "/"), $.expression)),
      prec.left(PREC.ADD, seq($.expression, choice("+", "-"), $.expression)),
      prec.left(PREC.COMPARISON, seq($.expression, choice("==", "!=", ">=", "<=", ">", "<", "is", seq("is", "not")), $.expression)),
      prec.left(PREC.AND, seq($.expression, "and", $.expression)),
      prec.left(PREC.OR, seq($.expression, "or", $.expression))
    ),
    ternary_expression: ($) => prec.right(PREC.TERNARY, seq($.expression, "?", $.expression, ":", $.expression)),

    match_expression: ($) => seq("match", field("subject", $.identifier), "{", repeat1($.match_arm), "}"),
    match_arm: ($) => seq($.match_pattern, ":", $.expression, optional("or")),
    match_pattern: ($) => choice("_", commaOrSep1($.identifier))
  }
});

function commaSep1(rule) {
  return seq(rule, repeat(seq(",", rule)));
}

function commaOrSep1(rule) {
  return seq(rule, repeat(seq("or", rule)));
}