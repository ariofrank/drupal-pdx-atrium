<?php
// $Id: spaces_handler_field_spaces_feature.inc,v 1.1.2.1.2.1.2.3 2009/12/17 22:33:05 alexb Exp $

/**
 * Field handler to translate a node type into its spaces feature.
 */
class spaces_handler_field_spaces_feature extends views_handler_field_node_type {
  function render($values) {
    static $map;
    static $features;
    if (!isset($map)) {
      $map = spaces_features_map('node');
      $features = spaces_features();
    }
    $output = '';
    if (isset($map[$values->{$this->field_alias}])) {
      $feature = $map[$values->{$this->field_alias}];
      $label = $features[$feature]->name;
      $output = "<span class='spaces-feature feature-{$feature}'>{$label}</span>";
    }
    return $output;
  }
}
