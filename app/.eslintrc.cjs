module.exports = {
  root: true,
  env: {
    es2022: true,
    node: true,
  },
  parser: 'vue-eslint-parser',
  parserOptions: {
    parser: '@typescript-eslint/parser',
    ecmaVersion: 'latest',
    sourceType: 'module',
    extraFileExtensions: ['.vue'],
  },
  extends: [
    'eslint:recommended',
    'plugin:vue/vue3-recommended',
    'plugin:@typescript-eslint/recommended',
  ],
  rules: {
    'no-undef': 'off',
    '@typescript-eslint/no-empty-object-type': 'off',
    'vue/multi-word-component-names': 'off',
  },
};
