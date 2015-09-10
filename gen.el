;; Prototype idea generating options parser

;; TODO : This should be generating code from ast rather than through

;; TODO : Write a reader macro for commonly occuring statements

;; raw code and indentation insertion.



(require 'cl)

(defvar *outfile-format* "%s-arg-parse.h")

(defun an-comma-sep(args)
  (let ((r "")
        (first t))    
    (dolist (a args r)
      (setq r (concat r (if (not first)",")  a))
      (if first
          (setq first nil)))))

(defun an-string-rep(a)
  (cond 
   ((stringp a) (format "\"%s\"" a))
   ((symbolp a) (format (symbol-name a)))
   (t (format "%s" a))))

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

(defstruct options-arg
  name 
  index 
  short long 
  usage required type default)

(defstruct options-parser name args)

(defun an-decl-type(type default)
  (cond 
   ((eq type 'string) "char* ")
   ((eq type 'int)  "int ")
   (t  default)))


(defun an-gen-config-struct(args)
  (insert "\nstruct config {\n")
  (dolist (arg (options-parser-args args))
    (let ((arg-type (options-arg-type arg))
          (arg-name (options-arg-name arg))
          (arg-default (options-arg-default arg)))
      (an-gen-line (an-decl-type arg-type arg-type) " " arg-name)))
  (insert "};\n"))

(defun an-gen-config-init(args)
  (insert "\n\nvoid config_init(struct config* cfg ) {\n")
  (dolist (arg (options-parser-args args))
    (let ((arg-type (options-arg-type arg))
          (arg-name (options-arg-name arg))
          (arg-default (options-arg-default arg)))
      (insert (format "\tcfg->%s = " arg-name))      
      (if arg-default
          (insert arg-default)
       (cond
        ((eql 'string arg-type) (insert "NULL"))
        ((eql 'int arg-type) (insert "0"))
        ))
      (insert ";\n")
      ))
  (insert "}\n\n"))


(defmacro an-c(c level)
    (cond 
     ((not c) `"")
     ((stringp c) ;; line insert
      `(an-line- ,level ,c))
     ((eq (car c) `do-while)      
      `(concat 
        (an-line- ,level "do {")
        (an-c ,(cadr c) ,(+ 1 level))
        (an-line- ,level (format "} while (%s)" ,(caddr c)))
        ))
     ((eq (car c) `if)
      `(concat 
        (an-line- ,level (format "if (%s) {" ,(cadr c)))
        (an-c ,(caddr c) ,(+ 1 level))
        (an-line- ,level "}")))
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
        (an-c ,(caddr c) ,level)))
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


(defun an-build-case(arg-short arg-name)
  (list 'case (format "'%s'" arg-short)
    (list 'block (format "cfg->%s = %s " arg-name "atoi(optarg)") "break;")))

(defun an-gen-case-statements(args)
  "Generate the case required case statements"
  (let ((r '()))
  (dolist (arg args)
    (let ((arg-type (options-arg-type arg))
          (arg-name (options-arg-name arg))
          (arg-long (options-arg-long arg))
          (arg-short (options-arg-short arg))
          (arg-default (options-arg-default arg)))      
      (if arg-short
            (setq r (cons (an-build-case arg-short arg-name) r))
        )))
  (nreverse r)))

(defmacro an-gen-parse-args2(uargs)
  (let ((args (eval uargs)))
    `(insert (an-c
      (progn         
        "void parse_args(int argc, char* argv[], struct config* cfg)"
        (block
            "config_init(cfg);"
            "extern int optind;"
            "extern char* optarg;"           
          (do-while 
           (progn 
             "next_opt = getopt_long(argc,argv,short_options,long_options,NULL);"
             (switch "next_opt" 
                     ,@(an-gen-case-statements args)

                     (case "\'?\'"  
                       (block 
                         "usage(stderr,-1);" 
                         "break;"))
                     (case "-1"  
                       (block  "break;"))
                     (default  
                       (block 
                         "printf(\"unexpected exit \");"
                         "abort();"))
                     ))
                     
          "next_opt != -1")
          "int remaining_args = argc - optind;"
          ,@(let ((r '()))
             (let* ((iargs (an-indexed-args2 args))
                    (num   (length iargs)))
               (dolist (arg iargs)
                 (setq r (cons (format "cfg->%s = argv[optind+%d]" (options-arg-name arg) (options-arg-index arg)) r))))
             (nreverse r))
)) 0))))


;;(an-gen-case-statements (options-parser-args tgrep-options))


(defun an-gen-parse-args(args)
  (insert "\n\nvoid parse_args(int argc, char* argv[], struct config* cfg ) {\n")
  (an-gen-line "config_init(cfg)")
  (an-gen-short-options args)
  (an-gen-long-options args)

  (an-gen-line "int next_opt")
  (an-gen-line "extern int optind")
  (an-gen-line "extern char* optarg")

  (an-gen-line- "do {")
  (an-gen-line "\tnext_opt = getopt_long(argc,argv,short_options,long_options,NULL)")  
  (an-gen-line- "switch(next_opt) {")
  (dolist (arg (options-parser-args args))
    (let ((arg-type (options-arg-type arg))
          (arg-name (options-arg-name arg))
          (arg-long (options-arg-long arg))
          (arg-short (options-arg-short arg))
          (arg-default (options-arg-default arg)))
      (if arg-short
          (progn 
          (an-gen-line- (format "case '%s':" arg-short))
          (insert "\t") ;; indent
          (an-gen-line (format "cfg->%s" arg-name) " = " 
                        "atoi(optarg)")
          (an-gen-line "\tbreak")))
      ))
  (an-gen-line "case '?':  usage(stderr,-1)")
  (an-gen-line "case -1:  break")
  (an-gen-line- "default: ")
  (an-gen-line "\tprintf(\"unexpected exit \")")
  (an-gen-line "\tabort()")
  (an-gen-line "} //switch")
  (an-gen-line "} while(next_opt != -1)")
  (an-gen-line "int remaining_args = argc - optind")

  (let* ((iargs (an-indexed-args args))
        (num   (length iargs)))
    
    (an-gen-line- (format "if(remaining_args < %d) {" num))
    (an-gen-line (format  "\tfprintf(stderr, \"Insufficient  %d  args required  \\n\");" num))
    (an-gen-line "\texit(-1)")
    (an-gen-line- "}")

    (dolist (arg iargs)
      (an-gen-line 
       (format "cfg->%s" (options-arg-name arg)) 
       " = "
       (format "argv[optind+%d]" (options-arg-index arg)))))

  (an-gen-line "return cfg")
  
  (insert "}\n\n"))


(defun an-indexed-args2(args)
  (let ((retval '()))
    (dolist (arg  args)
      (if (options-arg-index arg)
          (setf retval (cons arg retval))))
    (nreverse retval)))

(defun an-indexed-args(args)
  (let ((retval '()))
    (dolist (arg (options-parser-args args))
      (if (options-arg-index arg)
          (setf retval (cons arg retval))))
    (nreverse retval)))

(defun an-gen-long-options(args)
  (insert "\n\tconst struct option long_option[] = {\n")  
  (let ((first t))
  (dolist (arg (options-parser-args args))
    (let ((arg-type (options-arg-type arg))
          (arg-name (options-arg-name arg))
          (arg-long (options-arg-long arg))
          (arg-short (options-arg-short arg))
          (arg-default (options-arg-default arg)))
      (if arg-long
          (progn 
            (insert "\t\t")
            (if (not first)
                (insert ","))
            (insert "{" (format "\"%s\"" arg-long) ",1,NULL," (format "\"%s\""arg-short) "}\n")      
      (setq first nil)))))
  (insert "\t};\n")))

(defun an-gen-short-options(args)
  (insert "\tconst char* short_options = "  (format "\"%s\"" (an-args-to-short-options args)) ";\n\n"))

(defun an-args-to-short-options(args)
  (let ((retval ""))
  (dolist (arg (options-parser-args args))
    (let ((arg-type (options-arg-type arg))
          (arg-name (options-arg-name arg))
          (arg-default (options-arg-default arg))
          (arg-short (options-arg-short arg)))
      (if arg-short
      (setf retval 
            (concat retval                    
                    (concat arg-short 
                            (if arg-type 
                                 ":" "")))))))
  retval))    

(defun an-gen-usage(args)
  (an-gen-line "void usage(FILE* stream, int exit_code) {")

  )

(defun an-generate-parser(args)
  (let ((out-file (format *outfile-format* (options-parser-name args)  )))
    (save-window-excursion
      (find-file out-file)
      (delete-region 1 (point-max)) ;; empty file
      (an-gen-config-struct args)
      (insert "\n\n")
      (an-gen-config-init args)
      (insert "\n\n")
      (insert "\nstruct config cfg;\n")
      (an-gen-parse-args2 (options-parser-args args)))))


(defvar tgrep-options
   (make-options-parser 
    :name "tgrep" 
    :args 
    (list 
     (make-options-arg 
      :name "search_term"   :index 0
      :usage "" 
      :required t  :type 'string)
     (make-options-arg 
      :name "path"   :index 1
      :usage "" 
      :required t  :type 'string)
     (make-options-arg 
      :name "num_readers"  
      :short "r"  :long "num_readers"   
      :usage "Number of readers to run simultaneously" 
      :default "1"
      :required nil  :type 'int)
     (make-options-arg 
      :name "num_searchers"   
      :short "s"  :long "num_searchers"   
      :default "1"
      :usage "Number of searchers to run simultaneously" 
      :required nil  :type 'int)
     (make-options-arg 
      :name "debug"  
      :short "D"  :long "debug-level"   
      :usage "Debug verbosity" 
      :required nil  :type 'int)
     (make-options-arg 
      :name "iovec_block_size"  
      :short "b"  :long "block-size"   
      :usage "Block size for debug queue" 
      :required nil  :type 'int
      :default "IOVEC_BLOCK_SIZE")
     (make-options-arg 
      :name "iovec_queue_size"   
      :short "q"  :long "queue-size"   
      :usage "Queue for debug queue" 
      :required nil  :type 'int
      :default "FREE_IOVEC_QUEUE_SIZE")
     (make-options-arg 
      :name "path_type" 
      :short nil  :long nil
      :usage nil
      :required nil  :type "enum path_type"
      :default "path_type_file"))))

(defun gen-test()
  (interactive)
  (an-generate-parser tgrep-options))



