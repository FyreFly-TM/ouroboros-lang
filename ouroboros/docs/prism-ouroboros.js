// Prism.js language definition for Ouroboros
Prism.languages.ouroboros = {
    'comment': [
        {
            pattern: /(^|[^\\])\/\*[\s\S]*?(?:\*\/|$)/,
            lookbehind: true
        },
        {
            pattern: /(^|[^\\:])\/\/.*/,
            lookbehind: true,
            greedy: true
        }
    ],
    'string': {
        pattern: /"(?:\\.|[^\\"])*"/,
        greedy: true
    },
    'keyword': /\b(?:let|const|fn|function|return|if|else|while|for|true|false|class|new|import|public|private|protected|static|null|int|float|bool|string|void|print|struct|this|extends|super|break|continue)\b/,
    'boolean': /\b(?:true|false)\b/,
    'number': /\b0x[\da-f]+\b|(?:\b\d+(?:\.\d*)?|\B\.\d+)(?:e[+-]?\d+)?/i,
    'operator': /[<>]=?|[!=]=?=?|--?|\+\+?|&&?|\|\|?|[?*/~^%]/,
    'punctuation': /[{}[\];(),.:]/,
    'class-name': {
        pattern: /(\b(?:class|extends|new|struct)\s+)\w+/,
        lookbehind: true
    },
    'function': {
        pattern: /(\b(?:function|fn)\s+)\w+/,
        lookbehind: true
    },
    'builtin': /\b(?:print|to_string|string_concat|string_length|opengl_init|opengl_create_context|opengl_destroy_context|opengl_clear|opengl_draw_arrays|opengl_swap_buffers|vulkan_init|vulkan_cleanup|voxel_engine_create|voxel_create_world|voxel_render_frame|ml_engine_create|init_gui|draw_window|draw_label|draw_button|gui_message_loop|connect_to_server|http_get|register_event|trigger_event|set_timeout)\b/,
    'type': {
        pattern: /\b(?:int|float|bool|string|void)\s+(?=\w)/,
        alias: 'builtin'
    },
    'constant': /\b[A-Z_][A-Z0-9_]*\b/,
    'property': {
        pattern: /(\.)[\w$]+/,
        lookbehind: true
    }
};

// Copy string interpolation from JavaScript
Prism.languages.ouroboros['string'].inside = {
    'interpolation': {
        pattern: /\$\{(?:[^{}]|\{(?:[^{}]|\{[^}]*\})*\})+\}/,
        inside: {
            'interpolation-punctuation': {
                pattern: /^\$\{|\}$/,
                alias: 'punctuation'
            },
            rest: Prism.languages.ouroboros
        }
    }
};

// Add alias
Prism.languages.ouro = Prism.languages.ouroboros; 