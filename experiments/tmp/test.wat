(module
    (import "js" "memory" (memory 4))
    (import "js" "callback" (func $callback (param i32 i32)))
    (func (export "parse") (param $off i32) (param $len i32) (param $max i32) (result i32)
        (local $c i32)
        (local $big v128)
        (set_local $c (i32.const 0))
        local.get 0
        (v128.load)
        (loop $loop
            (call $callback (get_local $off) (i32.sub (get_local $len) (get_local $c)))
            (set_local $c (i32.add (get_local $c) (i32.const 1)))
            (br_if $loop (i32.lt_u (get_local $c) (get_local $max)))
        )
        (return (get_local $c))
    )
)