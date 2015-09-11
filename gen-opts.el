;; Prototype idea generating options parser
;; TODO : This should be generating code from ast rather than through
;; TODO : Write a reader macro for commonly occuring statements
;; raw code and indentation insertion.

;; How I Learned to Stop Worrying and Love the Loop Macro
;; http://www.ccs.neu.edu/home/shivers/papers/loop.pdf

(require 'cl)
(require 'c-gen)
(require 's)


(defvar *outfile-format* "%s-arg-parse.h")

(defstruct options-arg
  name index 
  short long 
  usage required type default)

(defstruct options-parser name args)

(defvar tgrep-options
   (make-options-parser 
    :name "tgrep" 
    :args 
    (list 
     (make-options-arg 
      :name "search_term"   :index 0
      :required t  :type 'string)
     (make-options-arg 
      :name "path"   :index 1
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
      :type "enum path_type"
      :default "path_type_file"))))


(defun an-filter(pred list)
  (let ((retval '()))
    (dolist (arg  args)
      (if (funcall pred arg)
          (setf retval (cons arg retval))))
    (nreverse retval)))

(defun an-indexed-args(args)
  (an-filter (lambda(a) (options-arg-index a)) args))

(defun args-struct-entry(arg)
  (let ((type (an-decl-type (options-arg-type arg)))
        (name (options-arg-name arg)))
  (s-lex-format "${type} ${name};")))

(defun args-struct-entries(args)
  (mapcar 'args-struct-entry args))

(defmacro an-gen-config-struct(uargs)
  (let ((args (eval uargs)))
    `(an-c 
      (progn 
        "struct config" 
        (block ,@(args-struct-entries args))
        ";") 0)))

(defun an-config-init-arg(arg)
  (let* ((default-value (options-arg-default arg))
         (name  (options-arg-name arg))
         (value (if default-value default-value
                  (an-type-default-value (options-arg-type arg)))))
    (s-lex-format "cfg->${name} = ${value};")))                       

(defmacro an-gen-config-init(uargs)
  (let ((args (eval uargs)))
    `(an-c 
      (defun "void config_init(struct config* cfg) "
        ,@(mapcar 'an-config-init-arg args))      
      0)))

(defun an-build-case(arg-short arg-name)
  (list 'case (s-lex-format "'${arg-short}'")
        (list 'block 
              (s-lex-format "cfg->${arg-name} = atoi(optarg) ;") 
              "break;")))

(defun an-gen-case-statements(args)
  "Generate the case required case statements"
  (loop for arg  in args
        if (options-arg-short arg)
        for arg-short = (options-arg-short arg)
        for arg-name = (options-arg-name arg)
        collect (an-build-case arg-short arg-name)))

(defmacro an-gen-parse-args(uargs)
  (let ((args (eval uargs)))
    `(an-c
       (defun 
         "void parse_args(int argc, char* argv[], struct config* cfg)"
            "config_init(cfg);"
            "extern int optind;"
            "extern char* optarg;"        
            ,(an-gen-short-options args)   
            ,(an-gen-long-options args)
            (do-while "next_opt != -1"
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
                       )                    
             )
            "int remaining_args = argc - optind;"
            ,@(let ((r '()))
                (let* ((indexed-args (an-indexed-args args))
                       (num   (length indexed-args)))
                  (setq r 
                        (cons `(if ,(format "remaining_args < %d" num)
                                   ,(format "printf(stderr, \"Insufficient number of args %d args required  \\n\");" num)
                                 "exit(-1)")
                              r))                                    
                  (dolist (arg indexed-args)
                    (setq r (cons (format "cfg->%s = argv[optind+%d];" (options-arg-name arg) (options-arg-index arg)) r)))
                  (nreverse r)))
            "return cfg;"
            ) 0)))

(defun an-gen-long-option-record(arg)
  (let ((arg-long (options-arg-long arg))
        (arg-short (options-arg-short arg)))  
    (s-lex-format "{\"${arg-long}\",1,NULL,\"${arg-short}\"}")))

(defun an-gen-long-options(args)
  (format "const struct option long_option[] = {%s};"                
      (loop for arg in  args 
            if (options-arg-long arg)
            collect (an-gen-long-option-record arg) into records
            finally return (s-join "," records))))

(defun an-gen-short-options(args)
  (let ((sopts (an-args-to-short-options args)))
    (s-lex-format "const char* short_options = \"${sopts}\";" )))

(defun an-args-to-short-options(args)
  (loop for arg in args
        for arg-short = (options-arg-short arg)
        for arg-type  = (options-arg-type arg)
        if arg-short
        concat (concat arg-short (if arg-type ":" ""))))      

 (defmacro an-gen-usage(uargs)
   (let ((args (eval uargs)))
     `(an-c 
       (defun "void usage(FILE* stream, int exit_code)"
         "fprintf(stream,\"Usage: tgrep  <options> [input file] \\n\");"
         (progn "struct description"
            (block
                "char* short_option;"
                "char* long_option;"
                "char* description;")
            ";"
            )
         ,(format 
           "const struct description desc_arr[] = {%s};"
             (loop for arg in args
                   if (and (options-arg-usage arg) (not (eql (options-arg-usage  arg)"")))
                   with fmt = (lambda(s l desc) 
                                (s-lex-format "{\"${s}\",\"--${l}\",\"${desc}\"}"))
                   for s = (options-arg-short arg)
                   for l = (options-arg-long arg)
                   for desc = (options-arg-usage arg)
                   for rec  = (funcall fmt s l desc)
                   collect rec into records
                   finally return (s-join "," records)))         
         "int i = 0;"
         (progn "for(i = 0; i < 4 ; i++)"
                (block
                    "fprintf(stream,\"%-10s%-14s\\t%-10s\\n\",desc_arr[i].short_option,desc_arr[i].long_option,desc_arr[i].description);"))
         "exit(exit_code);")
       0)))


;;(an-gen-usage (options-parser-args tgrep-options))

(defun an-generate-parser(args)
  (let ((out-file (format *outfile-format* (options-parser-name args)  )))
    (save-window-excursion
      (find-file out-file)
      (delete-region 1 (point-max)) ;; empty file
      (insert (an-gen-config-struct (options-parser-args args)))
      (insert "\n\n")
      (insert (an-gen-config-init (options-parser-args args)))
      (insert "\n\n")
      (insert "\nstruct config cfg;\n")
      (insert (an-gen-parse-args (options-parser-args args)))
      (insert (an-gen-usage (options-parser-args args)))
      )))

(defun gen-test()
  (interactive)
  (an-generate-parser tgrep-options))

(provide 'gen-opts)
