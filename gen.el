;; Prototype idea generating options parser

;; TODO : This should be generating code from ast rather than through
;; raw code and indentation insertion.

(require 'cl)

(defvar *outfile-format* "%s-arg-parse.h")

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
          (an-gen-line (format "cfg.%s" arg-name) " = " 
                        "atoi(optarg)")
          (an-gen-line "\tbreak")))
      ))
  
  (an-gen-line "} //switch")
  (an-gen-line "} while(next_opt != -1)")
  (an-gen-line "int remaining_args = argc - optind")

  (dolist (iopt (an-indexed-args args))
    (
  
  (insert "}\n\n"))

(defun an-indexed-args(args)
  (let ((retval '()))
    (dolist (arg (options-parser-args args))
      (if (options-arg-index arg)
          (setf retval (append retval arg)))) retval))

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
      (an-gen-parse-args args))))


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



