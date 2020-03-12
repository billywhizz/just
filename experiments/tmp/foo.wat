(module
    (import "js" "memory" (memory 4))
    (import "js" "callback" (func $callback (param i32)))
    (func (export "parse") (param $off i32) (param $len i32) (result i32)
        (local $target i32)
        (set_local $target (i32.const 168626701))
        (set_local $len (i32.sub (get_local $len) (i32.const 3)))
        (block $exit
          (loop $loop
              (call $callback (i32.load (get_local $off)))
              (set_local $off (i32.add (get_local $off) (i32.const 1)))
              (br_if $exit (i32.eq (get_local $target) (i32.load (get_local $off))))
              (br_if $loop (i32.lt_u (get_local $off) (get_local $len)))
          )
        )
        (return (get_local $off))
    )
)