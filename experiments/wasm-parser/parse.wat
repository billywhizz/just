(module
;;todo. store offsets in memory and don't do callbacks
    (import "js" "memory" (memory 16))
    (func (export "parse") (param $off i32) (param $len i32) (result i32)
        (local $offsets i32)
        (local $target i32)
        (local $count i32)
        (set_local $count (i32.const 0))
        (set_local $offsets (i32.const 4100))
        (set_local $target (i32.const 168626701))
        (set_local $len (i32.sub (get_local $len) (i32.const 3)))
        (block $exit
          (loop $loop
              (if 
                (i32.eq (get_local $target) (i32.load (get_local $off)))
                (then 
                  (i32.store (get_local $offsets) (get_local $off))
                  (set_local $offsets (i32.add (get_local $offsets) (i32.const 4)))
                  (set_local $count (i32.add (get_local $count) (i32.const 1)))
                )
              )
              (set_local $off (i32.add (get_local $off) (i32.const 1)))
              (br_if $loop (i32.lt_u (get_local $off) (get_local $len)))
                (set_local $off (i32.const 0))
          )
        )
        (return (get_local $count))
    )
)