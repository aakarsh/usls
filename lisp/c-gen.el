(require 's)

(defun an-comma-sep(args)
  (let ((r "")
        (first t))    
    (dolist (a args r)
      (setq r (concat r (if (not first)",")  a))
      (if first
          (setq first nil)))))

(defmacro an-string-rep-args(&rest args)
  (let ((res '()))
    (dolist (r args res)
      (setq res (cons  (an-string-rep r) res)))
    (setq res (nreverse res))
    (an-comma-sep res)))

(defun an-str-n(s times)  
  (let ((retval ""))
  (dotimes (i times)
    (setq retval (concat retval s))) retval))
      
(defmacro an-line(level &rest args)  
  `(concat ,(an-str-n "\t" level) ,@args ";\n"))

(defmacro an-line-(level &rest args)
  `(concat ,(an-str-n "\t" level) ,@args "\n"))

(defmacro an-line--(level &rest args)
  `(concat ,(an-str-n "\t" level) ,@args ))

(defmacro an-gen-line(&rest args)
  `(insert "\t" ,@args ";\n"))

(defmacro an-gen-line-(&rest args)
  `(insert "\t" ,@args "\n"))

(defun an-string-rep(a)
  (cond 
   ((stringp a) (format "\"%s\"" a))
   ((symbolp a) (format (symbol-name a)))
   (t (format "%s" a))))

(defun an-decl-type(type)
  (cond 
   ((eq type 'string) "char* ")
   ((eq type 'int)  "int ")
   ((stringp type) type)
   (t) ""))

(defun an-type-default-value(type)
  (cond 
   ((eq type 'string) "NULL")
   ((eq type 'int)  0)
   (t) ""))

(defmacro an-c(c level)
    (cond 
     ((not c) `"")
     ((stringp c) ;; line insert
      `(an-line- ,level ,c))
     ((eq (car c) `include)
      `(concat 
        (an-line- ,level (format "#include %s" ,(cadr c)))))
     ((eq (car c) `defun)
      `(an-c (progn ,(cadr c)
               (block
                   ,@(cddr c))) ,level))
     ((eq (car c) `do-while)      
      `(concat 
        (an-line- ,level "do")
        (an-c (block ,@(cddr c)) ,level)
        (an-line- ,level (format " while (%s);" ,(cadr c)))))     
     ((eq (car c) `if)
      `(concat 
        (an-line- ,level (format "if (%s) " ,(cadr c)))
        (an-c (block ,@(cddr c))  ,level)))
     ((eq (car c) 'switch)
      `(concat
        (an-line- ,level (format "switch(%s) { " ,(cadr c)))
        ,@(let ((r '()))
           (dolist (cs (cddr c) r)
               (setq r 
                     (cons 
                      (if (eq 'default (car cs))
                          `(concat 
                            (an-line- ,level "default:")
                            (an-c ,(cdr cs) ,(+ level 1)))
                        `(concat
                          (an-line- ,level (format "case %s:" ,(cadr cs)))
                          (an-c ,(cddr cs ) ,(+ level 1))))
                        r)))
           (nreverse r))
        (an-line ,level "}")))
     ((eq (car c) 'progn)
      `(concat 
        (an-c  ,(cadr c) ,level)
        (an-c ,(cddr c) ,level)))
     ((eq (car c) 'block)
     `(concat 
       (an-line- ,level "{")
       (an-c ,(cadr c)  ,(+ level 1))
       (an-c ,(cdr (cdr c)) ,(+ level 1))
       (an-line- ,level "}")))     
     ;; ((stringp (car c)) ;; raw insertion
     ;;  `(concat 
     ;;    (an-line ,level ,(car c))
     ;;    (an-c ,(cdr c) ,level)))
     ((listp c) ;; fall back on progn semantics
      `(concat 
        (an-c  ,(car c) ,level)
        (an-c ,(cdr c) ,level)))
     (t ;; assume function call  <- this is a bad idea
      `(concat
        (an-line ,level (format "%s(%s)" ,(an-string-rep (car c)) (an-string-rep-args ,@(cdr c))))))
     ;; method name
))

(provide 'c-gen)
