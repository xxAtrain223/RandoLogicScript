(comment) @comment
(string) @string
(number) @number
(boolean) @boolean

[
  "region"
  "extend"
  "extern"
  "define"
  "enum"
  "events"
  "locations"
  "exits"
  "match"
  "here"
] @keyword

[
  "and"
  "or"
  "not"
  "is"
  "=="
  "!="
  ">="
  "<="
  ">"
  "<"
  "+"
  "-"
  "*"
  "/"
  "="
  "->"
  "?"
] @operator

(region_declaration name: (identifier) @type)
(extend_region_declaration name: (identifier) @type)
(enum_declaration name: (identifier) @type)
(extern_enum_declaration name: (identifier) @type)
(define_declaration name: (identifier) @function)
(extern_define_declaration name: (identifier) @function)
(parameter name: (identifier) @variable.parameter)
(parameter type: (identifier) @type)
(extern_define_declaration return_type: (identifier) @type)
(enum_member name: (identifier) @constant)
(glob_pattern) @constant
(member_expression object: (identifier) @type)
(member_expression member: (identifier) @property)
(named_argument name: (identifier) @variable.parameter)
(entry name: (identifier) @property)
(region_data_entry key: (identifier) @property)